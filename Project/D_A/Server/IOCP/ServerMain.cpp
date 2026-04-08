// ServerMain.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <vector>

#include "Protocol.h"
#include "NetApi.h"
#include "LobbyService.h"


// ===========================================================================
// 전역 설정 및 변수
// ===========================================================================
static const char* LISTEN_IP = "0.0.0.0";
static const uint16_t LISTEN_PORT = 9000;
static const int RECV_BUF_SIZE = 4096;

static HANDLE g_iocp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static std::atomic<bool> g_running{ true };

enum class IOType : uint8_t { RECV, SEND };

// ===========================================================================
// 데이터 구조체
// ===========================================================================
struct PerIoContext {
    OVERLAPPED ol{};
    WSABUF wsaBuf{};
    IOType type = IOType::RECV;
    char buffer[RECV_BUF_SIZE]{};

    // Send용 버퍼
    std::vector<char> dynBuffer;
    size_t dynOffset = 0;

    void ResetRecv() {
        std::memset(&ol, 0, sizeof(ol));
        type = IOType::RECV;
        wsaBuf.buf = buffer;
        wsaBuf.len = RECV_BUF_SIZE;
    }
};

struct ClientContext {
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in addr{};

    PerIoContext recvCtx;
    std::atomic<long> ioRef{ 0 };
    std::atomic<bool> closing{ false };

    // 수신 누적 버퍼 (TCP 경계 처리용)
    std::vector<char> streamBuf;

    // 송신 큐
    SRWLOCK sendLock{};
    std::deque<std::vector<char>> sendQueue;
    bool sendInFlight = false;

    ClientContext(SOCKET s, const sockaddr_in& a) : sock(s), addr(a) {
        recvCtx.ResetRecv();
        streamBuf.reserve(8192);
        InitializeSRWLock(&sendLock);
    }
};

// ===========================================================================
// 글로벌 객체 및 헬퍼 함수
// ===========================================================================
void SendPacket(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen);

NetApi       g_net(SendPacket);
LobbyService g_lobby(g_net);

void AddIO(ClientContext* c) {
    c->ioRef.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseIO(ClientContext* c) {
    const long left = c->ioRef.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (left == 0 && c->closing.load(std::memory_order_acquire)) {
        closesocket(c->sock);
        delete c;
    }
}

// ===========================================================================
// 송신 로직
// ===========================================================================
void SendPacket(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen) {
    const uint16_t totalSize = static_cast<uint16_t>(sizeof(PacketHeader) + payloadLen);
    if (totalSize > MAX_PACKET_SIZE) return;

    std::vector<char> pkt(totalSize);
    PacketHeader hdr{};
    hdr.size = htons(totalSize);
    hdr.type = htons(type);

    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0 && payload) {
        std::memcpy(pkt.data() + sizeof(hdr), payload, payloadLen);
    }

    AcquireSRWLockExclusive(&c->sendLock);
    c->sendQueue.push_back(std::move(pkt));

    // 현재 진행 중인 Send가 없다면 바로 시작
    if (!c->sendInFlight) {
        c->sendInFlight = true;

        PerIoContext* sendCtx = new PerIoContext();
        sendCtx->type = IOType::SEND;
        sendCtx->dynBuffer = c->sendQueue.front();
        sendCtx->wsaBuf.buf = sendCtx->dynBuffer.data();
        sendCtx->wsaBuf.len = (ULONG)sendCtx->dynBuffer.size();

        AddIO(c);
        DWORD flags = 0;
        if (WSASend(c->sock, &sendCtx->wsaBuf, 1, NULL, flags, &sendCtx->ol, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                ReleaseIO(c);
                delete sendCtx;
                c->sendInFlight = false;
            }
        }
    }
    ReleaseSRWLockExclusive(&c->sendLock);
}

// ===========================================================================
// Worker Thread
// ===========================================================================
void WorkerThread() {
    while (g_running.load()) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ret = GetQueuedCompletionStatus(g_iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);

        if (!ret && overlapped == nullptr) continue;

        ClientContext* client = reinterpret_cast<ClientContext*>(completionKey);
        PerIoContext* ioCtx = CONTAINING_RECORD(overlapped, PerIoContext, ol);

        // [에러 및 종료 처리]
        if (!ret || (bytesTransferred == 0 && ioCtx->type == IOType::RECV)) {
            if (!client->closing.exchange(true)) {
                g_lobby.OnClientDisconnected(client);
                closesocket(client->sock);
            }
            ReleaseIO(client);
            if (ioCtx->type == IOType::SEND) delete ioCtx;
            continue;
        }

        // [수신 처리]
        if (ioCtx->type == IOType::RECV) {
            bool closeClient = false;

            // 1. 받은 데이터를 streamBuf에 누적
            client->streamBuf.insert(client->streamBuf.end(), ioCtx->buffer, ioCtx->buffer + bytesTransferred);

            // 2. 패킷 헤더(4바이트)를 읽을 수 있을 만큼 데이터가 쌓였는지 확인
            while (client->streamBuf.size() >= sizeof(PacketHeader)) {
                PacketHeader hdr;
                std::memcpy(&hdr, client->streamBuf.data(), sizeof(PacketHeader));

                uint16_t totalSize = ntohs(hdr.size);
                uint16_t type = ntohs(hdr.type);

                // 헤더 검증
                if (totalSize < sizeof(PacketHeader) || totalSize > MAX_PACKET_SIZE) {
                    if (!client->closing.exchange(true)) {
                        g_lobby.OnClientDisconnected(client);
                        closesocket(client->sock);
                    }
                    closeClient = true;
                    break;
                }

                // 3. 하나의 온전한 패킷이 다 들어왔다면?
                if (client->streamBuf.size() >= totalSize) {
                    uint16_t payloadLen = totalSize - static_cast<uint16_t>(sizeof(PacketHeader));
                    const char* payload = client->streamBuf.data() + sizeof(PacketHeader);

                    g_lobby.OnPacket(client, type, payload, payloadLen);

                    client->streamBuf.erase(client->streamBuf.begin(),
                        client->streamBuf.begin() + totalSize);
                }
                else {
                    // 패킷이 아직 덜 왔으면 루프 탈출해서 더 받음
                    break;
                }
            }

            // 비정상 헤더로 클라를 닫아야 하면, 현재 I/O만 정리하고 다음 GQCS로 넘어감
            if (closeClient) {
                ReleaseIO(client);
                continue;
            }

            // 4. 다음 데이터를 받기 위해 다시 Recv 요청
            ioCtx->ResetRecv();
            DWORD flags = 0;
            if (WSARecv(client->sock, &ioCtx->wsaBuf, 1, NULL, &flags, &ioCtx->ol, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != WSA_IO_PENDING) {
                    if (!client->closing.exchange(true)) {
                        g_lobby.OnClientDisconnected(client);
                        closesocket(client->sock);
                    }
                    ReleaseIO(client);
                }
            }
        }
        // [송신 처리]
        else if (ioCtx->type == IOType::SEND) {
            ioCtx->dynOffset += bytesTransferred;

            // 아직 덜 보낸 데이터가 있다면 (부분 전송 발생)
            if (ioCtx->dynOffset < ioCtx->dynBuffer.size()) {
                ioCtx->wsaBuf.buf = ioCtx->dynBuffer.data() + ioCtx->dynOffset;
                ioCtx->wsaBuf.len = (ULONG)(ioCtx->dynBuffer.size() - ioCtx->dynOffset);

                DWORD flags = 0;
                if (WSASend(client->sock, &ioCtx->wsaBuf, 1, NULL, flags, &ioCtx->ol, NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != WSA_IO_PENDING) {
                        ReleaseIO(client);
                        delete ioCtx;
                        AcquireSRWLockExclusive(&client->sendLock);
                        client->sendInFlight = false;
                        ReleaseSRWLockExclusive(&client->sendLock);
                    }
                }
            }
            // 다 보냈다면 큐에서 다음 패킷 확인
            else {
                delete ioCtx;

                AcquireSRWLockExclusive(&client->sendLock);
                client->sendQueue.pop_front();

                if (!client->sendQueue.empty()) {
                    PerIoContext* nextSend = new PerIoContext();
                    nextSend->type = IOType::SEND;
                    nextSend->dynBuffer = client->sendQueue.front();
                    nextSend->wsaBuf.buf = nextSend->dynBuffer.data();
                    nextSend->wsaBuf.len = (ULONG)nextSend->dynBuffer.size();

                    DWORD flags = 0;
                    if (WSASend(client->sock, &nextSend->wsaBuf, 1, NULL, flags, &nextSend->ol, NULL) == SOCKET_ERROR) {
                        if (WSAGetLastError() != WSA_IO_PENDING) {
                            ReleaseIO(client);
                            delete nextSend;
                            client->sendInFlight = false;
                        }
                    }
                }
                else {
                    client->sendInFlight = false;
                    ReleaseIO(client);
                }
                ReleaseSRWLockExclusive(&client->sendLock);
            }
        }
    }
}

// ===========================================================================
// Main Entry Point
// ===========================================================================
int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!g_iocp) return 1;

    // 워커 스레드 4개 실행
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back(WorkerThread);

    g_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LISTEN_PORT);
    inet_pton(AF_INET, LISTEN_IP, &serverAddr.sin_addr);

    bind(g_listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(g_listenSock, SOMAXCONN);

    printf("[Server] Listening on %s:%d...\n", LISTEN_IP, LISTEN_PORT);

    // Accept 루프
    while (g_running.load()) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        SOCKET clientSock = WSAAccept(g_listenSock, (sockaddr*)&clientAddr, &addrLen, NULL, 0);
        if (clientSock == INVALID_SOCKET) {
            Sleep(10);
            continue;
        }

        printf("[Server] Client Accepted: %d.%d.%d.%d\n",
            clientAddr.sin_addr.S_un.S_un_b.s_b1, clientAddr.sin_addr.S_un.S_un_b.s_b2,
            clientAddr.sin_addr.S_un.S_un_b.s_b3, clientAddr.sin_addr.S_un.S_un_b.s_b4);

        ClientContext* ctx = new ClientContext(clientSock, clientAddr);
        AddIO(ctx);

        CreateIoCompletionPort((HANDLE)clientSock, g_iocp, (ULONG_PTR)ctx, 0);

        AddIO(ctx);
        DWORD flags = 0;
        if (WSARecv(clientSock, &ctx->recvCtx.wsaBuf, 1, NULL, &flags, &ctx->recvCtx.ol, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                ReleaseIO(ctx);
                ReleaseIO(ctx);
                delete ctx;
                continue;
            }
        }

        g_lobby.OnClientAccepted(ctx);
    }

    g_running = false;
    for (auto& t : workers) if (t.joinable()) t.join();

    closesocket(g_listenSock);
    CloseHandle(g_iocp);
    WSACleanup();

    return 0;
}