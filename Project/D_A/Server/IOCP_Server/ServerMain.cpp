// ServerMain.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <process.h>     
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstdio>


// =========================
// Basic config
// =========================
static const char* LISTEN_IP = "0.0.0.0";
static const uint16_t LISTEN_PORT = 7777;
static const int RECV_BUF_SIZE = 4096;

// =========================
// Globals
// =========================
static HANDLE g_hIOCP = nullptr;

// =========================
// Helpers
// =========================
static void PrintWSAError(const char* tag)
{
    int err = WSAGetLastError();
    printf("[WSA ERR] %s : %d\n", tag, err);
}

static void PrintGLE(const char* tag)
{
    DWORD err = GetLastError();
    printf("[WIN ERR] %s : %lu\n", tag, err);
}

// ======================================================================
// ====== Session / IO Context structures (세션 구조 & Overlapped) ======
// ======================================================================
enum class IOType : int
{
    RECV = 1,
    SEND = 2,
};

struct PerIoContext
{
    WSAOVERLAPPED ol{};
    WSABUF        wbuf{};
    IOType        type{ IOType::RECV };
    char          buffer[RECV_BUF_SIZE]{};

    PerIoContext(IOType t)
        : type(t)
    {
        ZeroMemory(&ol, sizeof(ol));
        wbuf.buf = buffer;
        wbuf.len = RECV_BUF_SIZE;
    }

    void ResetOverlapped()
    {
        ZeroMemory(&ol, sizeof(ol));
    }
};

struct ClientContext
{
    SOCKET sock{ INVALID_SOCKET };

    std::atomic_long ioCount{ 0 };     // outstanding I/O count
    std::atomic_bool closing{ false }; // close started?

    PerIoContext* recvIo{ nullptr };   // 1개 recv는 재사용(완료 후 다시 post)

    ClientContext()
    {
        recvIo = new PerIoContext(IOType::RECV);
    }

    ~ClientContext()
    {
        delete recvIo;
        recvIo = nullptr;
    }
};

static void BeginClose(ClientContext* ctx)
{
    if (!ctx) return;

    bool expected = false;
    if (ctx->closing.compare_exchange_strong(expected, true))
    {
        if (ctx->sock != INVALID_SOCKET)
        {
            closesocket(ctx->sock);
            ctx->sock = INVALID_SOCKET;
        }
    }
}

static void TryDelete(ClientContext* ctx)
{
    // close가 시작됐고, outstanding I/O가 0이면 안전하게 delete
    if (!ctx) return;
    if (ctx->closing.load() && ctx->ioCount.load() == 0)
    {
        delete ctx;
    }
}

// ======================================================================
// =========== PostRecv / PostSend (IOCP에 I/O 등록) ====================
// ======================================================================
static bool PostRecv(ClientContext* ctx)
{
    if (!ctx || ctx->closing.load()) return false;

    DWORD flags = 0;
    DWORD bytes = 0;

    ctx->recvIo->ResetOverlapped();
    ctx->recvIo->wbuf.buf = ctx->recvIo->buffer;
    ctx->recvIo->wbuf.len = RECV_BUF_SIZE;
    ctx->recvIo->type = IOType::RECV;

    ctx->ioCount.fetch_add(1);

    int ret = WSARecv(
        ctx->sock,
        &ctx->recvIo->wbuf,
        1,
        &bytes,
        &flags,
        (LPWSAOVERLAPPED)&ctx->recvIo->ol,
        nullptr
    );

    if (ret == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            ctx->ioCount.fetch_sub(1);
            PrintWSAError("WSARecv");
            return false;
        }
    }
    return true;
}

static bool PostSend(ClientContext* ctx, const char* data, int len)
{
    if (!ctx || ctx->closing.load()) return false;
    if (len <= 0) return true;

    // send는 “요청마다 새 IO context 생성” (완료 시 worker에서 delete)
    PerIoContext* io = new PerIoContext(IOType::SEND);
    io->ResetOverlapped();
    io->type = IOType::SEND;

    // 너무 길면 잘라서 보내기
    int sendLen = (len > RECV_BUF_SIZE) ? RECV_BUF_SIZE : len;
    memcpy(io->buffer, data, sendLen);
    io->wbuf.buf = io->buffer;
    io->wbuf.len = (ULONG)sendLen;

    DWORD bytes = 0;

    ctx->ioCount.fetch_add(1);

    int ret = WSASend(
        ctx->sock,
        &io->wbuf,
        1,
        &bytes,
        0,
        (LPWSAOVERLAPPED)&io->ol,
        nullptr
    );

    if (ret == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            ctx->ioCount.fetch_sub(1);
            PrintWSAError("WSASend");
            delete io;
            return false;
        }
    }

    return true;
}

// ======================================================================
// =========== Worker thread loop (GQCS로 완료 통지 처리) ===============
// ======================================================================
static unsigned __stdcall WorkerThread(void*)
{
    while (true)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED pOv = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            g_hIOCP,
            &bytes,
            &key,
            &pOv,
            INFINITE
        );

        // 종료 신호(나중에 필요하면 PostQueuedCompletionStatus로 보냄)
        if (key == 0 && pOv == nullptr)
            break;

        ClientContext* ctx = (ClientContext*)key;
        PerIoContext* io = (PerIoContext*)pOv;

        if (!ctx || !io)
            continue;

        // 완료 통지 1건 처리했으니 outstanding 감소
        ctx->ioCount.fetch_sub(1);

        if (!ok)
        {
            // 소켓 에러/취소 등이 여기로 올 수 있음
            // 여기서는 세션 종료로 처리
            BeginClose(ctx);
            if (io->type == IOType::SEND) delete io;
            TryDelete(ctx);
            continue;
        }

        // bytes == 0 : 정상 종료
        if (bytes == 0 && io->type == IOType::RECV)
        {
            BeginClose(ctx);
            TryDelete(ctx);
            continue;
        }

        if (io->type == IOType::RECV)
        {
            // 받은 데이터 처리(일단 에코)
            // printf는 너무 많이 찍으면 병목이라 나중엔 로그 레벨/샘플링 권장
            printf("[RECV %lu] %.*s\n", bytes, (int)bytes, io->buffer);

            // 에코 send
            PostSend(ctx, io->buffer, (int)bytes);

            // 다음 recv 다시 걸기
            if (!PostRecv(ctx))
            {
                BeginClose(ctx);
            }
            TryDelete(ctx);
        }
        else if (io->type == IOType::SEND)
        {
            // send 완료: per-send io는 여기서 정리
            delete io;
            TryDelete(ctx);
        }
    }

    return 0;
}

// =========================
// main
// =========================
int main()
{
    // 1) Winsock init
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        PrintWSAError("WSAStartup");
        return 1;
    }

    // 2) Create listen socket
    SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listenSock == INVALID_SOCKET)
    {
        PrintWSAError("WSASocket(listen)");
        WSACleanup();
        return 1;
    }

    // bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    inet_pton(AF_INET, LISTEN_IP, &addr.sin_addr);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        PrintWSAError("bind");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // listen
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        PrintWSAError("listen");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // 3) Create IOCP
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!g_hIOCP)
    {
        PrintGLE("CreateIoCompletionPort(IOCP)");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // 4) Worker threads
    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    const unsigned int cpu = (si.dwNumberOfProcessors == 0) ? 1u : (unsigned int)si.dwNumberOfProcessors;
    const unsigned int workerCount = std::max(1u, cpu * 2u);

    std::vector<HANDLE> workers;
    workers.reserve(workerCount);

    for (unsigned int i = 0; i < workerCount; ++i)
    {
        unsigned int tid = 0;
        HANDLE h = (HANDLE)_beginthreadex(nullptr, 0, &WorkerThread, nullptr, 0, &tid);
        workers.push_back(h);
    }

    printf("[IOCP] Listen on %s:%d  (workers=%u)\n", LISTEN_IP, (int)LISTEN_PORT, workerCount);
    printf("[IOCP] Ctrl+C to quit.\n");

    // 5) Accept loop
    while (true)
    {
        sockaddr_in caddr{};
        int clen = sizeof(caddr);

        SOCKET clientSock = accept(listenSock, (sockaddr*)&caddr, &clen);
        if (clientSock == INVALID_SOCKET)
        {
            PrintWSAError("accept");
            continue;
        }

        // Nagle off
        BOOL on = TRUE;
        setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));

        // 세션 생성
        auto* ctx = new ClientContext();
        ctx->sock = clientSock;

        // ======================================================================
        // ========  accept 후 소켓을 IOCP에 연결(CompletionKey=ctx) ============
        // ======================================================================
        HANDLE h = CreateIoCompletionPort((HANDLE)clientSock, g_hIOCP, (ULONG_PTR)ctx, 0);
        if (!h)
        {
            PrintGLE("CreateIoCompletionPort(clientSock)");
            BeginClose(ctx);
            delete ctx; // pending I/O 없으니 즉시 delete
            continue;
        }

        // 첫 recv 등록
        if (!PostRecv(ctx))
        {
            BeginClose(ctx);
            TryDelete(ctx);
            continue;
        }

        char ipbuf[64]{};
        inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, sizeof(ipbuf));
        printf("[ACCEPT] %s:%d\n", ipbuf, (int)ntohs(caddr.sin_port));
    }

    WSACleanup();
    return 0;
}
