// ServerMain.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <new>
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
    case PacketType::D2L_MATCH_ABORT_NOTIFY: return "D2L_MATCH_ABORT_NOTIFY";
    case PacketType::L2D_MATCH_ABORT_ACK: return "L2D_MATCH_ABORT_ACK";
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
static constexpr std::size_t SEND_QUEUE_MAX_PACKETS = 128;
static constexpr std::size_t SEND_QUEUE_MAX_BYTES = 256u * 1024u;
static constexpr std::size_t DEFAULT_MAX_CONNECTIONS = 256;
static constexpr std::size_t DEFAULT_LOCAL_CONNECTION_RESERVE = 8;
static constexpr std::size_t DEFAULT_INBOUND_PACKETS_PER_SECOND = 128;
static constexpr std::size_t DEFAULT_AUTH_PACKETS_PER_CONNECTION = 16;
static constexpr std::size_t MAX_CONFIGURED_CONNECTIONS = 65535;
static constexpr std::size_t MAX_CONFIGURED_PACKET_RATE = 65535;

static HANDLE g_iocp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static std::atomic<bool> g_running{ true };
static std::atomic<bool> g_consoleShutdownStarted{ false };
static std::atomic<std::size_t> g_activeClientCount{ 0 };
static std::atomic<std::uint64_t> g_rejectedConnectionCount{ 0 };
static std::size_t g_maxConnections = DEFAULT_MAX_CONNECTIONS;
static std::size_t g_localConnectionReserve = DEFAULT_LOCAL_CONNECTION_RESERVE;
static std::size_t g_inboundPacketsPerSecond = DEFAULT_INBOUND_PACKETS_PER_SECOND;
static std::size_t g_authPacketsPerConnection = DEFAULT_AUTH_PACKETS_PER_CONNECTION;
static SRWLOCK g_listenSocketLock = SRWLOCK_INIT;

void CloseListenSocketOnce() {
    AcquireSRWLockExclusive(&g_listenSocketLock);
    const SOCKET listenSocket = g_listenSock;
    g_listenSock = INVALID_SOCKET;
    ReleaseSRWLockExclusive(&g_listenSocketLock);

    if (listenSocket != INVALID_SOCKET) {
        shutdown(listenSocket, SD_BOTH);
        closesocket(listenSocket);
    }
}

BOOL WINAPI ConsoleControlHandler(DWORD controlType) {
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (!g_consoleShutdownStarted.exchange(true, std::memory_order_acq_rel)) {
            g_running.store(false, std::memory_order_release);
        }
        return TRUE;
    default:
        return FALSE;
    }
}

void ReleaseAdmissionSlot() {
    std::size_t active = g_activeClientCount.load(std::memory_order_acquire);
    while (active > 0) {
        if (g_activeClientCount.compare_exchange_weak(
                active,
                active - 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            IOCP_TRACE("[ADMISSION] Slot released active=%zu\n", active - 1);
            return;
        }
    }

    IOCP_ERR("[ADMISSION][ERR] Slot release underflow prevented\n");
}

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
    bool admissionSlotOwned = false;

    PerIoContext recvCtx;
    std::atomic<long> ioRef{ 0 };
    std::atomic<bool> closing{ false };
    std::atomic<bool> baseRefReleased{ false };
    std::atomic<bool> sendBudgetExceeded{ false };
    SRWLOCK closeLock{};

    ULONGLONG inboundPacketWindowStartMs = GetTickCount64();
    std::size_t inboundPacketCount = 0;
    std::size_t authPacketCount = 0;

    // ���� ���� ���� (TCP ��� ó����)
    std::vector<char> streamBuf;

    // �۽� ť
    SRWLOCK sendLock{};
    std::deque<std::vector<char>> sendQueue;
    std::size_t sendQueueBytes = 0;
    bool sendInFlight = false;

    ClientContext(SOCKET s, const sockaddr_in& a, bool ownsAdmissionSlot)
        : sock(s), addr(a), admissionSlotOwned(ownsAdmissionSlot) {
        recvCtx.ResetRecv();
        streamBuf.reserve(8192);
        InitializeSRWLock(&closeLock);
        InitializeSRWLock(&sendLock);
    }

    ~ClientContext() {
        if (admissionSlotOwned) {
            admissionSlotOwned = false;
            ReleaseAdmissionSlot();
        }
    }
};

// ===========================================================================
// �۷ι� ��ü �� ���� �Լ�
// ===========================================================================
void SendPacket(ClientContext* c, uint16_t type, const void* payload, std::size_t payloadLen);

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

std::size_t GetEnvSizeOrDefault(
    const char* name,
    std::size_t fallback,
    std::size_t minimum,
    std::size_t maximum) {
    const std::string value = GetEnvStringOrDefault(name, "");
    if (value.empty()) {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (!end ||
        *end != '\0' ||
        parsed < static_cast<unsigned long long>(minimum) ||
        parsed > static_cast<unsigned long long>(maximum)) {
        IOCP_INFO("[Server][WARN] Invalid %s=%s. fallback=%zu range=%zu..%zu\n",
            name,
            value.c_str(),
            fallback,
            minimum,
            maximum);
        return fallback;
    }

    return static_cast<std::size_t>(parsed);
}

bool IsLocalConnection(SOCKET clientSock, const sockaddr_in& clientAddr) {
    const std::uint32_t peerAddress = ntohl(clientAddr.sin_addr.s_addr);
    if ((peerAddress & 0xFF000000u) == 0x7F000000u) {
        return true;
    }

    sockaddr_in localAddr{};
    int localAddrLength = sizeof(localAddr);
    if (getsockname(
            clientSock,
            reinterpret_cast<sockaddr*>(&localAddr),
            &localAddrLength) == SOCKET_ERROR) {
        return false;
    }

    return localAddr.sin_family == AF_INET &&
        localAddr.sin_addr.s_addr == clientAddr.sin_addr.s_addr;
}

bool TryAcquireAdmissionSlot(SOCKET clientSock, const sockaddr_in& clientAddr) {
    const bool isLocal = IsLocalConnection(clientSock, clientAddr);
    const std::size_t connectionLimit = isLocal
        ? g_maxConnections
        : g_maxConnections - g_localConnectionReserve;

    std::size_t active = g_activeClientCount.load(std::memory_order_acquire);
    while (active < connectionLimit) {
        if (g_activeClientCount.compare_exchange_weak(
                active,
                active + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }
    }

    const std::uint64_t rejectedTotal =
        g_rejectedConnectionCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (rejectedTotal == 1 || rejectedTotal % 64 == 0) {
        IOCP_INFO(
            "[ADMISSION] Connection rejected source=%s active=%zu limit=%zu rejectedTotal=%llu\n",
            isLocal ? "local" : "remote",
            active,
            connectionLimit,
            static_cast<unsigned long long>(rejectedTotal));
    }
    return false;
}

void ConfigureConnectionAdmission() {
    g_maxConnections = GetEnvSizeOrDefault(
        "MANAGER_IOCP_MAX_CONNECTIONS",
        DEFAULT_MAX_CONNECTIONS,
        1,
        MAX_CONFIGURED_CONNECTIONS);
    g_localConnectionReserve = GetEnvSizeOrDefault(
        "MANAGER_IOCP_LOCAL_RESERVE",
        DEFAULT_LOCAL_CONNECTION_RESERVE,
        0,
        MAX_CONFIGURED_CONNECTIONS);

    if (g_localConnectionReserve >= g_maxConnections) {
        const std::size_t adjustedReserve =
            g_maxConnections > 1 ? g_maxConnections - 1 : 0;
        IOCP_INFO(
            "[Server][WARN] MANAGER_IOCP_LOCAL_RESERVE=%zu must be below max=%zu. adjusted=%zu\n",
            g_localConnectionReserve,
            g_maxConnections,
            adjustedReserve);
        g_localConnectionReserve = adjustedReserve;
    }

    IOCP_INFO(
        "[ADMISSION] Limits max=%zu localReserve=%zu remoteLimit=%zu\n",
        g_maxConnections,
        g_localConnectionReserve,
        g_maxConnections - g_localConnectionReserve);
}

void ConfigureInboundLimits() {
    g_inboundPacketsPerSecond = GetEnvSizeOrDefault(
        "MANAGER_IOCP_PACKETS_PER_SECOND",
        DEFAULT_INBOUND_PACKETS_PER_SECOND,
        8,
        MAX_CONFIGURED_PACKET_RATE);
    g_authPacketsPerConnection = GetEnvSizeOrDefault(
        "MANAGER_IOCP_AUTH_PACKETS_PER_CONNECTION",
        DEFAULT_AUTH_PACKETS_PER_CONNECTION,
        1,
        MAX_CONFIGURED_PACKET_RATE);

    IOCP_INFO(
        "[TRANSPORT] Limits packetsPerSecond=%zu authPacketsPerConnection=%zu\n",
        g_inboundPacketsPerSecond,
        g_authPacketsPerConnection);
}

bool IsSupportedInboundPacketType(uint16_t type) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::C2S_PING:
    case PacketType::C2S_LOGIN_REQ:
    case PacketType::C2S_REGISTER_REQ:
    case PacketType::C2S_ROOM_LIST_REQ:
    case PacketType::C2S_ROOM_CREATE_REQ:
    case PacketType::C2S_ROOM_JOIN_REQ:
    case PacketType::C2S_ROOM_LEAVE_REQ:
    case PacketType::C2S_ROOM_READY_REQ:
    case PacketType::C2S_ROOM_START_REQ:
    case PacketType::D2L_MATCH_END_NOTIFY:
    case PacketType::D2L_SERVER_READY_NOTIFY:
    case PacketType::D2L_MATCH_ABORT_NOTIFY:
        return true;
    default:
        return false;
    }
}

bool IsFixedInboundPayloadSizeValid(uint16_t type, uint16_t payloadLen) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::C2S_PING:
    case PacketType::C2S_ROOM_LIST_REQ:
    case PacketType::C2S_ROOM_LEAVE_REQ:
    case PacketType::C2S_ROOM_START_REQ:
        return payloadLen == 0;
    case PacketType::C2S_ROOM_JOIN_REQ:
        return payloadLen == sizeof(uint32_t);
    case PacketType::C2S_ROOM_READY_REQ:
        return payloadLen == sizeof(uint8_t);
    case PacketType::D2L_SERVER_READY_NOTIFY:
        return payloadLen == 18;
    default:
        return true;
    }
}

bool ConsumeInboundPacketBudget(ClientContext* client, uint16_t type) {
    const ULONGLONG nowMs = GetTickCount64();
    if (nowMs < client->inboundPacketWindowStartMs ||
        nowMs - client->inboundPacketWindowStartMs >= 1000) {
        client->inboundPacketWindowStartMs = nowMs;
        client->inboundPacketCount = 0;
    }

    ++client->inboundPacketCount;
    if (client->inboundPacketCount > g_inboundPacketsPerSecond) {
        IOCP_ERR(
            "[TRANSPORT][LIMIT] Packet rate exceeded from=%d.%d.%d.%d count=%zu limit=%zu\n",
            client->addr.sin_addr.S_un.S_un_b.s_b1,
            client->addr.sin_addr.S_un.S_un_b.s_b2,
            client->addr.sin_addr.S_un.S_un_b.s_b3,
            client->addr.sin_addr.S_un.S_un_b.s_b4,
            client->inboundPacketCount,
            g_inboundPacketsPerSecond);
        return false;
    }

    const PacketType packetType = static_cast<PacketType>(type);
    if (packetType == PacketType::C2S_LOGIN_REQ ||
        packetType == PacketType::C2S_REGISTER_REQ) {
        ++client->authPacketCount;
        if (client->authPacketCount > g_authPacketsPerConnection) {
            IOCP_ERR(
                "[TRANSPORT][LIMIT] Auth request limit exceeded from=%d.%d.%d.%d count=%zu limit=%zu\n",
                client->addr.sin_addr.S_un.S_un_b.s_b1,
                client->addr.sin_addr.S_un.S_un_b.s_b2,
                client->addr.sin_addr.S_un.S_un_b.s_b3,
                client->addr.sin_addr.S_un.S_un_b.s_b4,
                client->authPacketCount,
                g_authPacketsPerConnection);
            return false;
        }
    }

    return true;
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
void SendPacket(ClientContext* c, uint16_t type, const void* payload, std::size_t payloadLen) {
    if (!c ||
        c->closing.load(std::memory_order_acquire) ||
        c->sendBudgetExceeded.load(std::memory_order_acquire)) {
        return;
    }

    constexpr std::size_t headerSize = sizeof(PacketHeader);
    constexpr std::size_t maxPayloadSize = PACKET_SIZE_MAX - headerSize;
    if (payloadLen > maxPayloadSize) {
        IOCP_ERR("[SEND-ERR] Oversize packet rejected type=%s(%u) payloadLen=%zu maxPayload=%zu\n",
            PacketTypeToString(type),
            type,
            payloadLen,
            maxPayloadSize);
        return;
    }
    if (payloadLen > 0 && !payload) {
        IOCP_ERR("[SEND-ERR] Null payload rejected type=%s(%u) payloadLen=%zu\n",
            PacketTypeToString(type),
            type,
            payloadLen);
        return;
    }

    const std::size_t totalSize = headerSize + payloadLen;
    const uint16_t wireTotalSize = static_cast<uint16_t>(totalSize);
    std::vector<char> pkt(totalSize);
    PacketHeader hdr{};
    hdr.size = htons(wireTotalSize);
    hdr.type = htons(type);

    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0) {
        std::memcpy(pkt.data() + sizeof(hdr), payload, payloadLen);
    }

    IOCP_TRACE("[SEND] to=%d.%d.%d.%d type=%s(%u) payloadLen=%zu totalSize=%zu\n",
        c->addr.sin_addr.S_un.S_un_b.s_b1,
        c->addr.sin_addr.S_un.S_un_b.s_b2,
        c->addr.sin_addr.S_un.S_un_b.s_b3,
        c->addr.sin_addr.S_un.S_un_b.s_b4,
        PacketTypeToString(type),
        type,
        payloadLen,
        totalSize);

    bool closeAfterUnlock = false;
    bool queueBudgetExceeded = false;
    PerIoContext* failedSendCtx = nullptr;

    AcquireSRWLockExclusive(&c->sendLock);
    if (c->closing.load(std::memory_order_acquire) ||
        c->sendBudgetExceeded.load(std::memory_order_acquire)) {
        ReleaseSRWLockExclusive(&c->sendLock);
        return;
    }

    const bool packetBudgetExceeded =
        c->sendQueue.size() >= SEND_QUEUE_MAX_PACKETS;
    const bool byteBudgetExceeded =
        totalSize > SEND_QUEUE_MAX_BYTES ||
        c->sendQueueBytes > SEND_QUEUE_MAX_BYTES - totalSize;

    if (packetBudgetExceeded || byteBudgetExceeded) {
        c->sendBudgetExceeded.store(true, std::memory_order_release);
        queueBudgetExceeded = true;

        IOCP_ERR("[SEND-LIMIT] Closing slow client to=%d.%d.%d.%d queuedPackets=%zu/%zu queuedBytes=%zu/%zu attempted=%zu type=%s(%u)\n",
            c->addr.sin_addr.S_un.S_un_b.s_b1,
            c->addr.sin_addr.S_un.S_un_b.s_b2,
            c->addr.sin_addr.S_un.S_un_b.s_b3,
            c->addr.sin_addr.S_un.S_un_b.s_b4,
            c->sendQueue.size(),
            SEND_QUEUE_MAX_PACKETS,
            c->sendQueueBytes,
            SEND_QUEUE_MAX_BYTES,
            totalSize,
            PacketTypeToString(type),
            type);
    }
    else {
        c->sendQueueBytes += totalSize;
        c->sendQueue.push_back(std::move(pkt));

        if (!c->sendInFlight) {
            c->sendInFlight = true;

            PerIoContext* sendCtx = new PerIoContext();
            sendCtx->type = IOType::SEND;
            sendCtx->dynBuffer = c->sendQueue.front();
            sendCtx->wsaBuf.buf = sendCtx->dynBuffer.data();
            sendCtx->wsaBuf.len = static_cast<ULONG>(sendCtx->dynBuffer.size());

            AddIO(c);
            DWORD flags = 0;
            if (WSASend(c->sock, &sendCtx->wsaBuf, 1, NULL, flags, &sendCtx->ol, NULL) == SOCKET_ERROR) {
                const int err = WSAGetLastError();
                if (err != WSA_IO_PENDING) {
                    IOCP_ERR("[SEND-ERR] to=%d.%d.%d.%d type=%s(%u) WSA=%d\n",
                        c->addr.sin_addr.S_un.S_un_b.s_b1,
                        c->addr.sin_addr.S_un.S_un_b.s_b2,
                        c->addr.sin_addr.S_un.S_un_b.s_b3,
                        c->addr.sin_addr.S_un.S_un_b.s_b4,
                        PacketTypeToString(type),
                        type,
                        err);

                    c->sendQueue.clear();
                    c->sendQueueBytes = 0;
                    c->sendInFlight = false;
                    failedSendCtx = sendCtx;
                    closeAfterUnlock = true;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&c->sendLock);

    if (queueBudgetExceeded) {
        MarkClientClosing(c, "SendQueueBudgetExceeded");
        return;
    }

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

                    if (!IsSupportedInboundPacketType(type)) {
                        IOCP_ERR(
                            "[TRANSPORT][ERR] Unsupported inbound packet from=%d.%d.%d.%d type=%u payloadLen=%u\n",
                            client->addr.sin_addr.S_un.S_un_b.s_b1,
                            client->addr.sin_addr.S_un.S_un_b.s_b2,
                            client->addr.sin_addr.S_un.S_un_b.s_b3,
                            client->addr.sin_addr.S_un.S_un_b.s_b4,
                            type,
                            payloadLen);
                        MarkClientClosing(client, "UnsupportedInboundPacket");
                        closeClient = true;
                        break;
                    }

                    if (!IsFixedInboundPayloadSizeValid(type, payloadLen)) {
                        IOCP_ERR(
                            "[TRANSPORT][ERR] Invalid fixed payload size from=%d.%d.%d.%d type=%s(%u) payloadLen=%u\n",
                            client->addr.sin_addr.S_un.S_un_b.s_b1,
                            client->addr.sin_addr.S_un.S_un_b.s_b2,
                            client->addr.sin_addr.S_un.S_un_b.s_b3,
                            client->addr.sin_addr.S_un.S_un_b.s_b4,
                            PacketTypeToString(type),
                            type,
                            payloadLen);
                        MarkClientClosing(client, "InvalidFixedPayloadSize");
                        closeClient = true;
                        break;
                    }

                    if (!ConsumeInboundPacketBudget(client, type)) {
                        MarkClientClosing(client, "InboundPacketBudgetExceeded");
                        closeClient = true;
                        break;
                    }

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

                    if (client->closing.load(std::memory_order_acquire)) {
                        closeClient = true;
                        break;
                    }

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
                        client->sendQueueBytes = 0;
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
                    const std::size_t completedPacketBytes =
                        client->sendQueue.front().size();
                    client->sendQueueBytes =
                        client->sendQueueBytes >= completedPacketBytes
                        ? client->sendQueueBytes - completedPacketBytes
                        : 0;
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
                            client->sendQueueBytes = 0;
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

    ConfigureConnectionAdmission();
    ConfigureInboundLimits();

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

    u_long nonBlocking = 1;
    if (ioctlsocket(g_listenSock, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        printf("[Server][ERR] failed to make listen socket non-blocking. WSA=%d\n",
            WSAGetLastError());
        closesocket(g_listenSock);
        g_listenSock = INVALID_SOCKET;
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    IOCP_INFO("[Server] Listening on %s:%d...\n", listenIp.c_str(), listenPort);

    if (!SetConsoleCtrlHandler(ConsoleControlHandler, TRUE)) {
        IOCP_ERR("[Server][ERR] SetConsoleCtrlHandler failed. GLE=%lu\n", GetLastError());
    }

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

        AcquireSRWLockShared(&g_listenSocketLock);
        const SOCKET listenSocket = g_listenSock;
        ReleaseSRWLockShared(&g_listenSocketLock);

        if (listenSocket == INVALID_SOCKET) {
            break;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);

        timeval acceptPollTimeout{};
        acceptPollTimeout.tv_usec = 100000;

        const int selectResult = select(0, &readSet, nullptr, nullptr, &acceptPollTimeout);
        if (selectResult == SOCKET_ERROR) {
            const int selectError = WSAGetLastError();
            if (!g_running.load(std::memory_order_acquire) || selectError == WSAENOTSOCK) {
                break;
            }
            printf("[Server][ERR] listen select failed. WSA=%d\n", selectError);
            Sleep(10);
            continue;
        }
        if (selectResult == 0 || !g_running.load(std::memory_order_acquire)) {
            continue;
        }

        SOCKET clientSock = WSAAccept(listenSocket, (sockaddr*)&clientAddr, &addrLen, NULL, 0);
        if (clientSock == INVALID_SOCKET) {
            const int acceptErr = WSAGetLastError();
            if (!g_running.load() || acceptErr == WSAENOTSOCK || acceptErr == WSAEINTR) {
                break;
            }
            if (acceptErr == WSAEWOULDBLOCK) {
                continue;
            }
            printf("[Server][ERR] WSAAccept failed. WSA=%d\n", acceptErr);
            Sleep(10);
            continue;
        }

        if (!TryAcquireAdmissionSlot(clientSock, clientAddr)) {
            shutdown(clientSock, SD_BOTH);
            closesocket(clientSock);
            continue;
        }

        IOCP_TRACE("[Server] Client Accepted: %d.%d.%d.%d\n",
            clientAddr.sin_addr.S_un.S_un_b.s_b1, clientAddr.sin_addr.S_un.S_un_b.s_b2,
            clientAddr.sin_addr.S_un.S_un_b.s_b3, clientAddr.sin_addr.S_un.S_un_b.s_b4);

        ClientContext* ctx = new (std::nothrow) ClientContext(clientSock, clientAddr, true);
        if (!ctx) {
            IOCP_ERR("[ADMISSION][ERR] ClientContext allocation failed active=%zu\n",
                g_activeClientCount.load(std::memory_order_acquire));
            ReleaseAdmissionSlot();
            shutdown(clientSock, SD_BOTH);
            closesocket(clientSock);
            continue;
        }

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
    if (g_consoleShutdownStarted.load(std::memory_order_acquire)) {
        IOCP_INFO("[Server] Console shutdown requested. Cleaning up dedicated servers...\n");
    }
    if (maintenanceThread.joinable()) maintenanceThread.join();
    for (size_t i = 0; i < workers.size(); ++i) {
        PostQueuedCompletionStatus(g_iocp, 0, 0, nullptr);
    }
    for (auto& t : workers) if (t.joinable()) t.join();

    g_lobby.ShutdownDedicatedServers();
    CloseListenSocketOnce();
    SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);
    CloseHandle(g_iocp);
    WSACleanup();

    return 0;
}
