#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <vector>
#include <memory>


#include "NetApi.h"
#include "LobbyService.h"
#include "Protocol.h"


static const char* LISTEN_IP = "0.0.0.0";
static const uint16_t LISTEN_PORT = 9000;

static const int RECV_BUF_SIZE = 4096;
static const int STREAM_BUF_LIMIT = 64 * 1024;
static const uint16_t MAX_PACKET_SIZE = 4096;

static HANDLE g_iocp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static std::atomic<bool> g_running{ true };

static std::unique_ptr<LobbyService> g_lobby;

static void PrintWSAError(const char* msg)
{
    const int err = WSAGetLastError();
    std::printf("[WSA] %s (err=%d)\n", msg, err);
}

enum class IOType : uint8_t { RECV, SEND };

struct PerIoContext
{
    OVERLAPPED ol{};
    WSABUF wsaBuf{};
    IOType type = IOType::RECV;

    char buffer[RECV_BUF_SIZE]{};

    std::vector<char> dynBuffer;
    size_t dynOffset = 0;
    size_t dynTotal = 0;

    void ResetRecv()
    {
        std::memset(&ol, 0, sizeof(ol));
        type = IOType::RECV;
        wsaBuf.buf = buffer;
        wsaBuf.len = RECV_BUF_SIZE;
    }
};

struct ClientContext
{
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in addr{};

    PerIoContext recvCtx;
    std::atomic<long> ioRef{ 0 };
    std::atomic<bool> closing{ false };

    std::vector<char> streamBuf;

    SRWLOCK sendLock{};
    std::deque<std::vector<char>> sendQueue;
    bool sendInFlight = false;

    ClientContext(SOCKET s, const sockaddr_in& a) : sock(s), addr(a)
    {
        recvCtx.ResetRecv();
        streamBuf.reserve(8192);
        InitializeSRWLock(&sendLock);
    }
};

static void AddIO(ClientContext* c) { c->ioRef.fetch_add(1, std::memory_order_relaxed); }

static void ReleaseIO(ClientContext* c)
{
    const long left = c->ioRef.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (left == 0 && c->closing.load(std::memory_order_acquire))
    {
        closesocket(c->sock);
        delete c;
    }
}

static void BeginClose(ClientContext* c)
{
    bool expected = false;
    if (c->closing.compare_exchange_strong(expected, true))
    {

        if (g_lobby)
            g_lobby->OnClientDisconnected(c);

        shutdown(c->sock, SD_BOTH);

        AcquireSRWLockExclusive(&c->sendLock);
        c->sendQueue.clear();
        c->sendInFlight = false;
        ReleaseSRWLockExclusive(&c->sendLock);

        if (c->ioRef.load(std::memory_order_acquire) == 0)
        {
            closesocket(c->sock);
            delete c;
        }
    }
}

static bool PostRecv(ClientContext* c)
{
    if (c->closing.load()) return false;

    c->recvCtx.ResetRecv();

    DWORD flags = 0;
    DWORD bytes = 0;

    AddIO(c);
    const int ret = WSARecv(c->sock, &c->recvCtx.wsaBuf, 1, &bytes, &flags, &c->recvCtx.ol, NULL);
    if (ret == SOCKET_ERROR)
    {
        const int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            ReleaseIO(c);
            PrintWSAError("WSARecv failed");
            return false;
        }
    }
    return true;
}

static bool PostSendInternal(ClientContext* c, PerIoContext* io)
{
    if (c->closing.load()) return false;

    const size_t remaining = (io->dynTotal > io->dynOffset) ? (io->dynTotal - io->dynOffset) : 0;
    if (remaining == 0) return true;

    std::memset(&io->ol, 0, sizeof(io->ol));
    io->type = IOType::SEND;
    io->wsaBuf.buf = io->dynBuffer.data() + io->dynOffset;
    io->wsaBuf.len = static_cast<ULONG>(remaining);

    DWORD bytes = 0;

    AddIO(c);
    const int ret = WSASend(c->sock, &io->wsaBuf, 1, &bytes, 0, &io->ol, NULL);
    if (ret == SOCKET_ERROR)
    {
        const int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            ReleaseIO(c);
            PrintWSAError("WSASend failed");
            return false;
        }
    }
    return true;
}

static bool StartSendWithBuffer(ClientContext* c, std::vector<char>&& buf)
{
    if (c->closing.load()) return false;
    if (buf.empty()) return true;

    auto* io = new PerIoContext();
    io->type = IOType::SEND;
    io->dynBuffer = std::move(buf);
    io->dynOffset = 0;
    io->dynTotal = io->dynBuffer.size();

    if (!PostSendInternal(c, io))
    {
        delete io;

        AcquireSRWLockExclusive(&c->sendLock);
        c->sendInFlight = false;
        c->sendQueue.clear();
        ReleaseSRWLockExclusive(&c->sendLock);

        BeginClose(c);
        return false;
    }
    return true;
}

static void KickNextSendIfAny(ClientContext* c)
{
    if (c->closing.load()) return;

    std::vector<char> next;
    bool hasNext = false;

    AcquireSRWLockExclusive(&c->sendLock);
    if (!c->sendQueue.empty())
    {
        next = std::move(c->sendQueue.front());
        c->sendQueue.pop_front();
        hasNext = true;
    }
    else
    {
        c->sendInFlight = false;
    }
    ReleaseSRWLockExclusive(&c->sendLock);

    if (hasNext)
    {
        StartSendWithBuffer(c, std::move(next));
    }
}

static bool QueueSendPacket(ClientContext* c, std::vector<char>&& packet)
{
    if (c->closing.load()) return false;

    std::vector<char> first;
    bool startNow = false;

    AcquireSRWLockExclusive(&c->sendLock);
    if (!c->sendInFlight)
    {
        c->sendInFlight = true;
        startNow = true;
        first = std::move(packet);
    }
    else
    {
        c->sendQueue.emplace_back(std::move(packet));
    }
    ReleaseSRWLockExclusive(&c->sendLock);

    if (startNow)
        return StartSendWithBuffer(c, std::move(first));

    return true;
}

static void SendPacket(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen)
{
    const uint16_t totalSize = static_cast<uint16_t>(sizeof(PacketHeader) + payloadLen);
    if (totalSize > MAX_PACKET_SIZE)
    {
        BeginClose(c);
        return;
    }

    std::vector<char> pkt;
    pkt.resize(totalSize);

    PacketHeader hdr{};
    hdr.size = htons(totalSize);
    hdr.type = htons(type);

    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0 && payload)
        std::memcpy(pkt.data() + sizeof(hdr), payload, payloadLen);

    QueueSendPacket(c, std::move(pkt));
}

static void DispatchPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen)
{
    if (!g_lobby)
    {
        BeginClose(c);
        return;
    }

    if (!g_lobby->OnPacket(c, type, payload, payloadLen))
    {
        BeginClose(c);
        return;
    }
}

static void ProcessStreamBuffer(ClientContext* c)
{
    while (true)
    {
        if (c->streamBuf.size() < sizeof(PacketHeader))
            return;

        PacketHeader hdr{};
        std::memcpy(&hdr, c->streamBuf.data(), sizeof(hdr));

        const uint16_t pktSize = ntohs(hdr.size);
        const uint16_t pktType = ntohs(hdr.type);

        if (pktSize < sizeof(PacketHeader) || pktSize > MAX_PACKET_SIZE)
        {
            BeginClose(c);
            return;
        }

        if (c->streamBuf.size() < pktSize)
            return;

        const char* payload = c->streamBuf.data() + sizeof(PacketHeader);
        const uint16_t payloadLen = static_cast<uint16_t>(pktSize - sizeof(PacketHeader));

        DispatchPacket(c, pktType, payload, payloadLen);

        if (c->closing.load())
            return;

        c->streamBuf.erase(c->streamBuf.begin(), c->streamBuf.begin() + pktSize);
    }
}

static void AppendAndProcess(ClientContext* c, const char* data, int len)
{
    if (len <= 0) return;

    if (c->streamBuf.size() + static_cast<size_t>(len) > STREAM_BUF_LIMIT)
    {
        BeginClose(c);
        return;
    }

    const size_t old = c->streamBuf.size();
    c->streamBuf.resize(old + static_cast<size_t>(len));
    std::memcpy(c->streamBuf.data() + old, data, static_cast<size_t>(len));

    ProcessStreamBuffer(c);
}



static void AcceptLoop()
{
    while (g_running.load())
    {
        sockaddr_in cliAddr{};
        int addrLen = sizeof(cliAddr);

        SOCKET cliSock = accept(g_listenSock, reinterpret_cast<sockaddr*>(&cliAddr), &addrLen);
        if (cliSock == INVALID_SOCKET)
        {
            if (!g_running.load())
                break;

            PrintWSAError("accept failed");
            continue;
        }

        auto* c = new ClientContext(cliSock, cliAddr);

        HANDLE h = CreateIoCompletionPort(reinterpret_cast<HANDLE>(cliSock), g_iocp, reinterpret_cast<ULONG_PTR>(c), 0);
        if (h == NULL)
        {
            PrintWSAError("CreateIoCompletionPort(client) failed");
            BeginClose(c);
            continue;
        }

        char ipStr[64]{};
        inet_ntop(AF_INET, &cliAddr.sin_addr, ipStr, sizeof(ipStr));
        std::printf("[ACCEPT] %s:%d\n", ipStr, ntohs(cliAddr.sin_port));

        if (!PostRecv(c))
        {
            BeginClose(c);
            continue;
        }

        if (g_lobby)
            g_lobby->OnClientConnected(c);

    }
}

static void WorkerLoop()
{
    while (true)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* pOl = nullptr;

        const BOOL ok = GetQueuedCompletionStatus(g_iocp, &bytes, &key, &pOl, INFINITE);

        if (key == 0 && pOl == nullptr)
            break;

        if (key == 0 || pOl == nullptr)
            continue;

        auto* c = reinterpret_cast<ClientContext*>(key);
        auto* io = reinterpret_cast<PerIoContext*>(pOl);

        if (!ok)
        {
            const DWORD gle = GetLastError();
            (void)gle;
            if (bytes == 0)
            {
                ReleaseIO(c);
                BeginClose(c);
                if (io->type == IOType::SEND)
                    delete io;
                continue;
            }
        }

        if (bytes == 0)
        {
            ReleaseIO(c);
            BeginClose(c);
            if (io->type == IOType::SEND)
                delete io;
            continue;
        }

        if (io->type == IOType::RECV)
        {
            const int recvLen = static_cast<int>(bytes);
            AppendAndProcess(c, io->buffer, recvLen);

            ReleaseIO(c);

            if (!PostRecv(c))
                BeginClose(c);
        }
        else
        {
            io->dynOffset += bytes;

            if (io->dynOffset < io->dynTotal)
            {
                PostSendInternal(c, io);
                ReleaseIO(c);
            }
            else
            {
                delete io;
                ReleaseIO(c);

                KickNextSendIfAny(c);
            }
        }
    }
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT)
    {
        g_running.store(false);

        if (g_listenSock != INVALID_SOCKET)
        {
            closesocket(g_listenSock);
            g_listenSock = INVALID_SOCKET;
        }

        return TRUE;
    }
    return FALSE;
}

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        PrintWSAError("WSAStartup failed");
        return 1;
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    NetApi_Bind(&SendPacket, &BeginClose);
    g_lobby = std::make_unique<LobbyService>(g_net);

    g_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (g_listenSock == INVALID_SOCKET)
    {
        PrintWSAError("WSASocket failed");
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    inet_pton(AF_INET, LISTEN_IP, &addr.sin_addr);

    if (bind(g_listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        PrintWSAError("bind failed");
        closesocket(g_listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(g_listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        PrintWSAError("listen failed");
        closesocket(g_listenSock);
        WSACleanup();
        return 1;
    }

    std::printf("[LISTEN] %s:%d\n", LISTEN_IP, LISTEN_PORT);

    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (g_iocp == NULL)
    {
        PrintWSAError("CreateIoCompletionPort(IOCP) failed");
        closesocket(g_listenSock);
        WSACleanup();
        return 1;
    }

    unsigned workerCount = std::thread::hardware_concurrency();
    if (workerCount == 0) workerCount = 4;

    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (unsigned i = 0; i < workerCount; ++i)
        workers.emplace_back(WorkerLoop);

    std::thread acceptThread(AcceptLoop);

    while (g_running.load())
        Sleep(50);

    if (acceptThread.joinable())
        acceptThread.join();

    for (unsigned i = 0; i < workerCount; ++i)
        PostQueuedCompletionStatus(g_iocp, 0, 0, NULL);

    for (auto& t : workers)
        t.join();

    CloseHandle(g_iocp);
    WSACleanup();
    std::printf("[SHUTDOWN]\n");
    return 0;
}
