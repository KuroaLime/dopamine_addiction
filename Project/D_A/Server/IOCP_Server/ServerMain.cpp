#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>


static const char* LISTEN_IP = "0.0.0.0";
static const uint16_t LISTEN_PORT = 9000;
static const int RECV_BUF_SIZE = 4096;

static HANDLE g_iocp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static std::atomic<bool> g_running{ true };

static void PrintWSAError(const char* msg)
{
    const int err = WSAGetLastError();
    std::printf("[WSA] %s (err=%d)\n", msg, err);
}

enum class IOType : uint8_t { RECV, SEND };

struct PerIoContext
{
    OVERLAPPED ol{};
    WSABUF     wsaBuf{};
    IOType     type = IOType::RECV;
    char       buffer[RECV_BUF_SIZE]{};

    char* dynBuf = nullptr;
    int   dynTotal = 0;
    int   dynOffset = 0;

    void Reset(IOType t)
    {
        std::memset(&ol, 0, sizeof(ol));
        type = t;

        if (t == IOType::RECV)
        {
            wsaBuf.buf = buffer;
            wsaBuf.len = RECV_BUF_SIZE;
        }
        else
        {
            wsaBuf.buf = nullptr;
            wsaBuf.len = 0;
        }
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

    ClientContext(SOCKET s, const sockaddr_in& a) : sock(s), addr(a)
    {
        recvCtx.Reset(IOType::RECV);

        streamBuf.reserve(RECV_BUF_SIZE * 2);
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
        shutdown(c->sock, SD_BOTH);

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

    c->recvCtx.Reset(IOType::RECV);

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

static void DestroySendIo(PerIoContext* io)
{
    if (!io) return;
    if (io->dynBuf)
    {
        delete[] io->dynBuf;
        io->dynBuf = nullptr;
    }
    delete io;
}

static bool PostSendInternal(ClientContext* c, PerIoContext* io)
{
    if (c->closing.load()) return false;

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

static bool PostSend(ClientContext* c, const char* data, int len)
{
    if (c->closing.load()) return false;
    if (len <= 0) return true;

    auto* io = new PerIoContext();
    io->Reset(IOType::SEND);

    io->dynTotal = len;
    io->dynOffset = 0;
    io->dynBuf = new char[len];
    std::memcpy(io->dynBuf, data, len);

    io->wsaBuf.buf = io->dynBuf;
    io->wsaBuf.len = static_cast<ULONG>(len);

    if (!PostSendInternal(c, io))
    {
        DestroySendIo(io);
        return false;
    }
    return true;
}


static const size_t STREAM_BUF_LIMIT = 1024 * 1024;
static const uint16_t MAX_PAYLOAD_SIZE = 8192;

static bool SendPacket(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    if (payloadLen > MAX_PAYLOAD_SIZE) return false;

    const uint16_t netLen = htons(payloadLen);
    const int totalLen = 2 + payloadLen;

    std::vector<char> pkt;
    pkt.resize(totalLen);

    std::memcpy(pkt.data(), &netLen, 2);
    if (payloadLen > 0)
        std::memcpy(pkt.data() + 2, payload, payloadLen);

    return PostSend(c, pkt.data(), static_cast<int>(pkt.size()));
}

static void HandlePacket(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    SendPacket(c, payload, payloadLen);
}

static void ProcessStreamBuffer(ClientContext* c)
{
    while (true)
    {
        if (c->streamBuf.size() < 2)
            return;

        uint16_t netLen = 0;
        std::memcpy(&netLen, c->streamBuf.data(), 2);
        const uint16_t payloadLen = ntohs(netLen);

        if (payloadLen > MAX_PAYLOAD_SIZE)
        {
            BeginClose(c);
            return;
        }

        const size_t need = 2ull + payloadLen;
        if (c->streamBuf.size() < need)
            return;

        const char* payload = c->streamBuf.data() + 2;
        HandlePacket(c, payload, payloadLen);

        c->streamBuf.erase(c->streamBuf.begin(), c->streamBuf.begin() + need);
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

        auto* c = reinterpret_cast<ClientContext*>(key);
        auto* io = reinterpret_cast<PerIoContext*>(pOl);

        if (!ok)
        {
            const DWORD gle = GetLastError();
            (void)gle;

            if (io && io->type == IOType::SEND)
                DestroySendIo(io);

            ReleaseIO(c);
            BeginClose(c);
            continue;
        }

        if (bytes == 0)
        {
            if (io && io->type == IOType::SEND)
                DestroySendIo(io);

            ReleaseIO(c);
            BeginClose(c);
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
            io->dynOffset += static_cast<int>(bytes);

            if (io->dynOffset < io->dynTotal)
            {
                std::memset(&io->ol, 0, sizeof(io->ol));
                io->wsaBuf.buf = io->dynBuf + io->dynOffset;
                io->wsaBuf.len = static_cast<ULONG>(io->dynTotal - io->dynOffset);

                const bool posted = PostSendInternal(c, io);
                ReleaseIO(c);

                if (!posted)
                {
                    DestroySendIo(io);
                    BeginClose(c);
                }
            }
            else
            {
                DestroySendIo(io);
                ReleaseIO(c);
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
