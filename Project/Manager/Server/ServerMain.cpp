// ServerMain.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <vector>

#include "Protocol_D.h"
#include "NetApi.h"
#include "LobbyService.h"

#ifndef MANAGER_IOCP_TRACE_LOG
#define MANAGER_IOCP_TRACE_LOG 0
#endif

#define IOCP_TRACE(...) do { if (MANAGER_IOCP_TRACE_LOG) { printf(__VA_ARGS__); } } while (0)
#define IOCP_INFO(...)  do { printf(__VA_ARGS__); } while (0)
#define IOCP_ERR(...)   do { printf(__VA_ARGS__); } while (0)

// ===========================================================================
// ������ ���
// ===========================================================================
const char* PacketTypeToString(uint16_t type) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::C2S_PING:            return "C2S_PING";
    case PacketType::S2C_PONG:            return "S2C_PONG";
    case PacketType::S2C_WELCOME:         return "S2C_WELCOME";

    case PacketType::C2S_LOGIN_REQ:       return "C2S_LOGIN_REQ";
    case PacketType::S2C_LOGIN_RES:       return "S2C_LOGIN_RES";
    case PacketType::C2S_REGISTER_REQ:    return "C2S_REGISTER_REQ";
    case PacketType::S2C_REGISTER_RES:    return "S2C_REGISTER_RES";

    case PacketType::C2S_ROOM_LIST_REQ:   return "C2S_ROOM_LIST_REQ";
    case PacketType::S2C_ROOM_LIST_RES:   return "S2C_ROOM_LIST_RES";

    case PacketType::C2S_ROOM_CREATE_REQ: return "C2S_ROOM_CREATE_REQ";
    case PacketType::S2C_ROOM_CREATE_RES: return "S2C_ROOM_CREATE_RES";

    case PacketType::C2S_ROOM_JOIN_REQ:   return "C2S_ROOM_JOIN_REQ";
    case PacketType::S2C_ROOM_JOIN_RES:   return "S2C_ROOM_JOIN_RES";

    case PacketType::C2S_ROOM_LEAVE_REQ:  return "C2S_ROOM_LEAVE_REQ";
    case PacketType::S2C_ROOM_LEAVE_RES:  return "S2C_ROOM_LEAVE_RES";

    case PacketType::C2S_ROOM_READY_REQ:  return "C2S_ROOM_READY_REQ";
    case PacketType::S2C_ROOM_READY_BRD:  return "S2C_ROOM_READY_BRD";

    case PacketType::C2S_ROOM_START_REQ:  return "C2S_ROOM_START_REQ";
    case PacketType::S2C_ROOM_START_RES:  return "S2C_ROOM_START_RES";

    case PacketType::S2C_GAME_START:      return "S2C_GAME_START";
    case PacketType::D2L_MATCH_END_NOTIFY: return "D2L_MATCH_END_NOTIFY";
    case PacketType::D2L_SERVER_READY_NOTIFY: return "D2L_SERVER_READY_NOTIFY";
    case PacketType::L2D_MATCH_END_ACK: return "L2D_MATCH_END_ACK";
    case PacketType::L2D_SERVER_READY_ACK: return "L2D_SERVER_READY_ACK";
    default:                              return "UNKNOWN_PACKET";
    }
}

// ===========================================================================
// ���� ���� �� ����
// ===========================================================================
static const char* DEFAULT_LISTEN_IP = "0.0.0.0";
static const char* FALLBACK_LISTEN_IP = "127.0.0.1";
static const uint16_t DEFAULT_LISTEN_PORT = 9000;
static const int RECV_BUF_SIZE = 4096;

static HANDLE g_iocp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static std::atomic<bool> g_running{ true };

enum class IOType : uint8_t { RECV, SEND };

// ===========================================================================
// ������ ����ü
// ===========================================================================
struct PerIoContext {
    OVERLAPPED ol{};
    WSABUF wsaBuf{};
    IOType type = IOType::RECV;
    char buffer[RECV_BUF_SIZE]{};

    // Send�� ����
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
    std::atomic<bool> baseRefReleased{ false };
    SRWLOCK closeLock{};

    // ���� ���� ���� (TCP ��� ó����)
    std::vector<char> streamBuf;

    // �۽� ť
    SRWLOCK sendLock{};
    std::deque<std::vector<char>> sendQueue;
    bool sendInFlight = false;

    ClientContext(SOCKET s, const sockaddr_in& a) : sock(s), addr(a) {
        recvCtx.ResetRecv();
        streamBuf.reserve(8192);
        InitializeSRWLock(&closeLock);
        InitializeSRWLock(&sendLock);
    }
};

// ===========================================================================
// �۷ι� ��ü �� ���� �Լ�
// ===========================================================================
void SendPacket(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen);

NetApi       g_net(SendPacket);
LobbyService g_lobby(g_net);

std::string TrimCopy(std::string value) {
    const char* whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return {};
    }

    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string GetEnvStringOrDefault(const char* name, const char* fallback) {
    const DWORD requiredSize = GetEnvironmentVariableA(name, nullptr, 0);
    if (requiredSize == 0) {
        return fallback ? fallback : "";
    }

    std::string value(requiredSize, '\0');
    const DWORD copiedSize = GetEnvironmentVariableA(name, value.data(), requiredSize);
    if (copiedSize == 0 || copiedSize >= requiredSize) {
        return fallback ? fallback : "";
    }

    value.resize(copiedSize);
    value = TrimCopy(value);
    return value.empty() ? (fallback ? fallback : "") : value;
}

uint16_t GetEnvPortOrDefault(const char* name, uint16_t fallback) {
    const std::string value = GetEnvStringOrDefault(name, "");
    if (value.empty()) {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed == 0 || parsed > 65535) {
        printf("[Server][WARN] Invalid %s=%s. fallback=%u\n",
            name,
            value.c_str(),
            static_cast<unsigned>(fallback));
        return fallback;
    }

    return static_cast<uint16_t>(parsed);
}

bool BuildListenAddress(const std::string& ip, uint16_t port, sockaddr_in& outAddr) {
    std::memset(&outAddr, 0, sizeof(outAddr));
    outAddr.sin_family = AF_INET;
    outAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &outAddr.sin_addr) != 1) {
        printf("[Server][ERR] inet_pton failed. ip=%s WSA=%d\n",
            ip.c_str(),
            WSAGetLastError());
        return false;
    }

    return true;
}

void AddIO(ClientContext* c) {
    c->ioRef.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseIO(ClientContext* c);

void CloseClientSocketOnce(ClientContext* c) {
    if (!c) {
        return;
    }

    AcquireSRWLockExclusive(&c->closeLock);

    const SOCKET sock = c->sock;
    if (sock == INVALID_SOCKET) {
        ReleaseSRWLockExclusive(&c->closeLock);
        return;
    }

    c->sock = INVALID_SOCKET;
    ReleaseSRWLockExclusive(&c->closeLock);

    shutdown(sock, SD_BOTH);
    closesocket(sock);
}

void MarkClientClosing(ClientContext* c, const char* reason) {
    if (!c) {
        return;
    }

    const bool firstClose = !c->closing.exchange(true, std::memory_order_acq_rel);
    if (firstClose) {
        IOCP_TRACE("[CONN] CLOSE reason=%s to=%d.%d.%d.%d\n",
            reason ? reason : "Unknown",
            c->addr.sin_addr.S_un.S_un_b.s_b1,
            c->addr.sin_addr.S_un.S_un_b.s_b2,
            c->addr.sin_addr.S_un.S_un_b.s_b3,
            c->addr.sin_addr.S_un.S_un_b.s_b4);
        g_lobby.OnClientDisconnected(c);
    }

    CloseClientSocketOnce(c);

    if (firstClose &&
        !c->baseRefReleased.exchange(true, std::memory_order_acq_rel)) {
        ReleaseIO(c);
    }
}

void ReleaseIO(ClientContext* c) {
    const long left = c->ioRef.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (left == 0 && c->closing.load(std::memory_order_acquire)) {
        CloseClientSocketOnce(c);
        delete c;
    }
}

// ===========================================================================
// �۽� ����
// ===========================================================================
void SendPacket(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen) {
    if (!c || c->closing.load(std::memory_order_acquire)) {
        return;
    }

    const uint16_t totalSize = static_cast<uint16_t>(sizeof(PacketHeader) + payloadLen);
    if (totalSize > PACKET_SIZE_MAX) return;

    std::vector<char> pkt(totalSize);
    PacketHeader hdr{};
    hdr.size = htons(totalSize);
    hdr.type = htons(type);

    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0 && payload) {
        std::memcpy(pkt.data() + sizeof(hdr), payload, payloadLen);
    }

    // ������ ���
    IOCP_TRACE("[SEND] to=%d.%d.%d.%d type=%s(%u) payloadLen=%u totalSize=%u\n",
        c->addr.sin_addr.S_un.S_un_b.s_b1,
        c->addr.sin_addr.S_un.S_un_b.s_b2,
        c->addr.sin_addr.S_un.S_un_b.s_b3,
        c->addr.sin_addr.S_un.S_un_b.s_b4,
        PacketTypeToString(type),
        type,
        payloadLen,
        totalSize);

    bool closeAfterUnlock = false;
    PerIoContext* failedSendCtx = nullptr;

    AcquireSRWLockExclusive(&c->sendLock);
    if (c->closing.load(std::memory_order_acquire)) {
        ReleaseSRWLockExclusive(&c->sendLock);
        return;
    }

    c->sendQueue.push_back(std::move(pkt));

    // ���� ���� ���� Send�� ���ٸ� �ٷ� ����
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
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                printf("[SEND-ERR] to=%d.%d.%d.%d type=%s(%u) WSA=%d\n",
                    c->addr.sin_addr.S_un.S_un_b.s_b1,
                    c->addr.sin_addr.S_un.S_un_b.s_b2,
                    c->addr.sin_addr.S_un.S_un_b.s_b3,
                    c->addr.sin_addr.S_un.S_un_b.s_b4,
                    PacketTypeToString(type),
                    type,
                    err);

                c->sendQueue.clear();
                c->sendInFlight = false;
                failedSendCtx = sendCtx;
                closeAfterUnlock = true;
            }
        }
    }
    ReleaseSRWLockExclusive(&c->sendLock);

    if (closeAfterUnlock) {
        MarkClientClosing(c, "WSASendStartFailed");
        ReleaseIO(c);
        delete failedSendCtx;
    }
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

        if (overlapped == nullptr) {
            if (!g_running.load()) {
                break;
            }
            continue;
        }

        ClientContext* client = reinterpret_cast<ClientContext*>(completionKey);
        PerIoContext* ioCtx = CONTAINING_RECORD(overlapped, PerIoContext, ol);

        // [���� �� ���� ó��]
        if (!ret || (bytesTransferred == 0 && ioCtx->type == IOType::RECV)) {
            MarkClientClosing(client, ret ? "PeerClosed" : "IocpCompletionFailed");
            ReleaseIO(client);
            if (ioCtx->type == IOType::SEND) delete ioCtx;
            continue;
        }

        // [���� ó��]
        if (ioCtx->type == IOType::RECV) {
            bool closeClient = false;

            // 1. ���� �����͸� streamBuf�� ����
            client->streamBuf.insert(client->streamBuf.end(), ioCtx->buffer, ioCtx->buffer + bytesTransferred);

            // 2. ��Ŷ ���(4����Ʈ)�� ���� �� ���� ��ŭ �����Ͱ� �׿����� Ȯ��
            while (client->streamBuf.size() >= sizeof(PacketHeader)) {
                PacketHeader hdr;
                std::memcpy(&hdr, client->streamBuf.data(), sizeof(PacketHeader));

                uint16_t totalSize = ntohs(hdr.size);
                uint16_t type = ntohs(hdr.type);

                // ��� ����
                if (totalSize < sizeof(PacketHeader) || totalSize > PACKET_SIZE_MAX) {
                    MarkClientClosing(client, "InvalidPacketSize");
                    closeClient = true;
                    break;
                }

                // 3. �ϳ��� ������ ��Ŷ�� �� ���Դٸ�?
                if (client->streamBuf.size() >= totalSize) {
                    uint16_t payloadLen = totalSize - static_cast<uint16_t>(sizeof(PacketHeader));
                    const char* payload = client->streamBuf.data() + sizeof(PacketHeader);

                    IOCP_TRACE("[RECV] from=%d.%d.%d.%d type=%s(%u) payloadLen=%u totalSize=%u\n",
                        client->addr.sin_addr.S_un.S_un_b.s_b1,
                        client->addr.sin_addr.S_un.S_un_b.s_b2,
                        client->addr.sin_addr.S_un.S_un_b.s_b3,
                        client->addr.sin_addr.S_un.S_un_b.s_b4,
                        PacketTypeToString(type),
                        type,
                        payloadLen,
                        totalSize);

                    g_lobby.OnPacket(client, type, payload, payloadLen);

                    client->streamBuf.erase(client->streamBuf.begin(),
                        client->streamBuf.begin() + totalSize);
                }
                else {
                    // ��Ŷ�� ���� �� ������ ���� Ż���ؼ� �� ����
                    break;
                }
            }

            // ������ ����� Ŭ�� �ݾƾ� �ϸ�, ���� I/O�� �����ϰ� ���� GQCS�� �Ѿ
            if (closeClient) {
                ReleaseIO(client);
                continue;
            }

            // 4. ���� �����͸� �ޱ� ���� �ٽ� Recv ��û
            ioCtx->ResetRecv();
            DWORD flags = 0;
            if (WSARecv(client->sock, &ioCtx->wsaBuf, 1, NULL, &flags, &ioCtx->ol, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != WSA_IO_PENDING) {
                    MarkClientClosing(client, "WSARecvRepostFailed");
                    ReleaseIO(client);
                }
            }
        }
        // [�۽� ó��]
        else if (ioCtx->type == IOType::SEND) {
            ioCtx->dynOffset += bytesTransferred;

            // ���� �� ���� �����Ͱ� �ִٸ� (�κ� ���� �߻�)
            if (ioCtx->dynOffset < ioCtx->dynBuffer.size()) {
                ioCtx->wsaBuf.buf = ioCtx->dynBuffer.data() + ioCtx->dynOffset;
                ioCtx->wsaBuf.len = (ULONG)(ioCtx->dynBuffer.size() - ioCtx->dynOffset);

                DWORD flags = 0;
                if (WSASend(client->sock, &ioCtx->wsaBuf, 1, NULL, flags, &ioCtx->ol, NULL) == SOCKET_ERROR) {
                    const int sendErr = WSAGetLastError();
                    if (sendErr != WSA_IO_PENDING) {
                        printf("[SEND-ERR] partial retry failed to=%d.%d.%d.%d WSA=%d\n",
                            client->addr.sin_addr.S_un.S_un_b.s_b1,
                            client->addr.sin_addr.S_un.S_un_b.s_b2,
                            client->addr.sin_addr.S_un.S_un_b.s_b3,
                            client->addr.sin_addr.S_un.S_un_b.s_b4,
                            sendErr);

                        AcquireSRWLockExclusive(&client->sendLock);
                        client->sendQueue.clear();
                        client->sendInFlight = false;
                        ReleaseSRWLockExclusive(&client->sendLock);

                        MarkClientClosing(client, "WSASendPartialRetryFailed");
                        ReleaseIO(client);
                        delete ioCtx;
                        continue;
                    }
                }
            }
            // �� ���´ٸ� ť���� ���� ��Ŷ Ȯ��
            else {
                delete ioCtx;

                bool releaseSendRef = false;
                bool closeAfterUnlock = false;
                PerIoContext* failedNextSend = nullptr;

                AcquireSRWLockExclusive(&client->sendLock);
                if (!client->sendQueue.empty()) {
                    client->sendQueue.pop_front();
                }

                if (!client->sendQueue.empty()) {
                    PerIoContext* nextSend = new PerIoContext();
                    nextSend->type = IOType::SEND;
                    nextSend->dynBuffer = client->sendQueue.front();
                    nextSend->wsaBuf.buf = nextSend->dynBuffer.data();
                    nextSend->wsaBuf.len = (ULONG)nextSend->dynBuffer.size();

                    DWORD flags = 0;
                    if (WSASend(client->sock, &nextSend->wsaBuf, 1, NULL, flags, &nextSend->ol, NULL) == SOCKET_ERROR) {
                        const int sendErr = WSAGetLastError();
                        if (sendErr != WSA_IO_PENDING) {
                            printf("[SEND-ERR] next send failed to=%d.%d.%d.%d WSA=%d\n",
                                client->addr.sin_addr.S_un.S_un_b.s_b1,
                                client->addr.sin_addr.S_un.S_un_b.s_b2,
                                client->addr.sin_addr.S_un.S_un_b.s_b3,
                                client->addr.sin_addr.S_un.S_un_b.s_b4,
                                sendErr);

                            client->sendQueue.clear();
                            client->sendInFlight = false;
                            failedNextSend = nextSend;
                            releaseSendRef = true;
                            closeAfterUnlock = true;
                        }
                    }
                }
                else {
                    client->sendInFlight = false;
                    releaseSendRef = true;
                }
                ReleaseSRWLockExclusive(&client->sendLock);

                if (closeAfterUnlock) {
                    MarkClientClosing(client, "WSASendNextFailed");
                    delete failedNextSend;
                }

                if (releaseSendRef) {
                    ReleaseIO(client);
                }
            }
        }
    }
}

// ===========================================================================
// Main Entry Point
// ===========================================================================
int main() {
    WSADATA wsa{};
    const int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaStartupResult != 0) {
        printf("[Server][ERR] WSAStartup failed. WSA=%d\n", wsaStartupResult);
        return 1;
    }

    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!g_iocp) {
        printf("[Server][ERR] CreateIoCompletionPort root failed. GLE=%lu\n", GetLastError());
        WSACleanup();
        return 1;
    }

    g_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (g_listenSock == INVALID_SOCKET) {
        printf("[Server][ERR] WSASocket listen failed. WSA=%d\n", WSAGetLastError());
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    std::string listenIp = GetEnvStringOrDefault("MANAGER_IOCP_BIND_IP", DEFAULT_LISTEN_IP);
    const uint16_t listenPort = GetEnvPortOrDefault("MANAGER_IOCP_PORT", DEFAULT_LISTEN_PORT);

    sockaddr_in serverAddr{};
    bool listenAddrReady = BuildListenAddress(listenIp, listenPort, serverAddr);
    if (!listenAddrReady && listenIp != FALLBACK_LISTEN_IP) {
        printf("[Server][WARN] Falling back bind ip to %s from %s\n",
            FALLBACK_LISTEN_IP,
            listenIp.c_str());
        listenIp = FALLBACK_LISTEN_IP;
        listenAddrReady = BuildListenAddress(listenIp, listenPort, serverAddr);
    }

    if (!listenAddrReady) {
        closesocket(g_listenSock);
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    if (bind(g_listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        const int firstBindErr = WSAGetLastError();
        printf("[Server][ERR] bind failed. ip=%s port=%u WSA=%d\n",
            listenIp.c_str(),
            static_cast<unsigned>(listenPort),
            firstBindErr);

        if (listenIp != FALLBACK_LISTEN_IP &&
            BuildListenAddress(FALLBACK_LISTEN_IP, listenPort, serverAddr)) {
            printf("[Server][WARN] Retrying bind on fallback ip=%s port=%u\n",
                FALLBACK_LISTEN_IP,
                static_cast<unsigned>(listenPort));
            listenIp = FALLBACK_LISTEN_IP;
        }

        if (listenIp != FALLBACK_LISTEN_IP ||
            bind(g_listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            printf("[Server][ERR] fallback bind failed. ip=%s port=%u WSA=%d\n",
                listenIp.c_str(),
                static_cast<unsigned>(listenPort),
                WSAGetLastError());
            closesocket(g_listenSock);
            CloseHandle(g_iocp);
            WSACleanup();
            return 1;
        }
    }

    if (listen(g_listenSock, SOMAXCONN) == SOCKET_ERROR) {
        printf("[Server][ERR] listen failed. ip=%s port=%u WSA=%d\n",
            listenIp.c_str(),
            static_cast<unsigned>(listenPort),
            WSAGetLastError());
        closesocket(g_listenSock);
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    IOCP_INFO("[Server] Listening on %s:%d...\n", listenIp.c_str(), listenPort);

    // ��Ŀ ������ 4�� ����
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back(WorkerThread);
    std::thread maintenanceThread([]() {
        while (g_running.load(std::memory_order_acquire)) {
            g_lobby.TickMaintenance();
            for (int i = 0; i < 10 && g_running.load(std::memory_order_acquire); ++i) {
                Sleep(100);
            }
        }
    });

    // Accept ����
    while (g_running.load()) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        SOCKET clientSock = WSAAccept(g_listenSock, (sockaddr*)&clientAddr, &addrLen, NULL, 0);
        if (clientSock == INVALID_SOCKET) {
            const int acceptErr = WSAGetLastError();
            if (!g_running.load() || acceptErr == WSAENOTSOCK || acceptErr == WSAEINTR) {
                break;
            }
            printf("[Server][ERR] WSAAccept failed. WSA=%d\n", acceptErr);
            Sleep(10);
            continue;
        }

        IOCP_TRACE("[Server] Client Accepted: %d.%d.%d.%d\n",
            clientAddr.sin_addr.S_un.S_un_b.s_b1, clientAddr.sin_addr.S_un.S_un_b.s_b2,
            clientAddr.sin_addr.S_un.S_un_b.s_b3, clientAddr.sin_addr.S_un.S_un_b.s_b4);

        ClientContext* ctx = new ClientContext(clientSock, clientAddr);

        HANDLE associatedIocp = CreateIoCompletionPort((HANDLE)clientSock, g_iocp, (ULONG_PTR)ctx, 0);
        if (!associatedIocp) {
            printf("[Server][ERR] CreateIoCompletionPort client failed. WSA=%d GLE=%lu\n",
                WSAGetLastError(),
                GetLastError());
            closesocket(clientSock);
            delete ctx;
            continue;
        }

        AddIO(ctx);
        AddIO(ctx);
        DWORD flags = 0;
        if (WSARecv(clientSock, &ctx->recvCtx.wsaBuf, 1, NULL, &flags, &ctx->recvCtx.ol, NULL) == SOCKET_ERROR) {
            const int recvErr = WSAGetLastError();
            if (recvErr != WSA_IO_PENDING) {
                printf("[Server][ERR] Initial WSARecv failed. WSA=%d\n", recvErr);
                ctx->closing.store(true, std::memory_order_release);
                CloseClientSocketOnce(ctx);
                ReleaseIO(ctx);
                ReleaseIO(ctx);
                continue;
            }
        }

        g_lobby.OnClientAccepted(ctx);
    }

    g_running = false;
    if (maintenanceThread.joinable()) maintenanceThread.join();
    for (size_t i = 0; i < workers.size(); ++i) {
        PostQueuedCompletionStatus(g_iocp, 0, 0, nullptr);
    }
    for (auto& t : workers) if (t.joinable()) t.join();

    closesocket(g_listenSock);
    CloseHandle(g_iocp);
    WSACleanup();

    return 0;
}
