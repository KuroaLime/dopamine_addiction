// LobbyService.cpp

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LobbyService.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <string>
#include <cstdlib>
#include <random>
#include <utility>

#ifndef MANAGER_LOBBY_TRACE_LOG
#define MANAGER_LOBBY_TRACE_LOG 0
#endif

#define LOBBY_TRACE(...) do { if (MANAGER_LOBBY_TRACE_LOG) { printf(__VA_ARGS__); } } while (0)
#define LOBBY_INFO(...)  do { printf(__VA_ARGS__); } while (0)
#define LOBBY_WARN(...)  do { printf(__VA_ARGS__); } while (0)
#define LOBBY_ERR(...)   do { printf(__VA_ARGS__); } while (0)


// ===========================================================================
// Dedicated Server Settings
// ===========================================================================

static const char* DEDI_EDITOR_ENV =
"MANAGER_UE_EDITOR_EXE";

static const char* DEDI_PACKAGED_SERVER_ENV =
"MANAGER_DEDI_SERVER_EXE";

static const char* DEDI_PROJECT_ENV =
"MANAGER_UPROJECT_PATH";

static const char* DEDI_WORKING_DIR_ENV =
"MANAGER_DEDI_WORKING_DIR";

static const char* DEDI_EXE_DEFAULT_PATH =
"C:\\Program Files\\Epic Games\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor.exe";

static const char* DEDI_EXE_REL_PATH =
"..\\..\\..\\..\\..\\UE\\UE_5.7_Source\\Engine\\Binaries\\Win64\\UnrealEditor.exe";

static const char* DEDI_EXE_FALLBACK_PATH =
"S:\\UE\\UE_5.7_Source\\Engine\\Binaries\\Win64\\UnrealEditor.exe";

static const char* DEDI_PROJECT_DEFAULT_PATH =
"X:\\Project\\Manager\\Manager.uproject";

static const char* DEDI_WORKING_DIR_DEFAULT_PATH =
"X:\\Project\\Manager";

static const char* DEDI_PROJECT_REL_PATH =
"..\\..\\..\\Manager.uproject";

static const char* DEDI_WORKING_DIR_REL_PATH =
"..\\..\\..";

static const char* DEDI_MAP_PATH =
"/Game/InGame/System/Main_Game_World";

static const char* DEDI_PUBLIC_IP_FALLBACK = "127.0.0.1";
static constexpr uint16_t IOCP_CALLBACK_PORT_FALLBACK = 9000;

static constexpr size_t MIN_PLAYERS_TO_START = 1;
static constexpr int DEDI_READY_TIMEOUT_SECONDS = 600;
static constexpr int IOCP_GHOST_RECONNECT_GRACE_SECONDS = 120;
static constexpr DWORD DEDI_TERMINATE_WAIT_MS = 5000;

static std::string TrimCopy(std::string value)
{
    const char* whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return {};
    }

    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

static std::string GetEnvStringOrDefault(const char* name, const char* fallback)
{
    const DWORD requiredSize = GetEnvironmentVariableA(name, nullptr, 0);
    if (requiredSize == 0)
    {
        return fallback ? fallback : "";
    }

    std::string value(requiredSize, '\0');
    const DWORD copiedSize = GetEnvironmentVariableA(name, value.data(), requiredSize);
    if (copiedSize == 0 || copiedSize >= requiredSize)
    {
        return fallback ? fallback : "";
    }

    value.resize(copiedSize);
    value = TrimCopy(value);
    return value.empty() ? (fallback ? fallback : "") : value;
}

static uint16_t GetEnvPortOrDefault(const char* name, uint16_t fallback)
{
    const std::string value = GetEnvStringOrDefault(name, "");
    if (value.empty())
    {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed == 0 || parsed > 65535)
    {
        LOBBY_WARN("[CONFIG][WARN] Invalid %s=%s fallback=%u\n",
            name,
            value.c_str(),
            static_cast<unsigned>(fallback));
        return fallback;
    }

    return static_cast<uint16_t>(parsed);
}

static std::string GetDediAdvertisedHost()
{
    std::string host = GetEnvStringOrDefault("MANAGER_DEDI_ADVERTISED_HOST", "");
    if (host.empty())
    {
        host = GetEnvStringOrDefault("MANAGER_DEDI_PUBLIC_IP", "");
    }

    return host.empty() ? DEDI_PUBLIC_IP_FALLBACK : host;
}

static std::string GetIocpCallbackHost()
{
    std::string host = GetEnvStringOrDefault("MANAGER_IOCP_CALLBACK_HOST", "");
    if (host.empty())
    {
        host = GetEnvStringOrDefault("MANAGER_IOCP_HOST", "");
    }

    return host.empty() ? DEDI_PUBLIC_IP_FALLBACK : host;
}

static uint16_t GetIocpCallbackPort()
{
    return GetEnvPortOrDefault("MANAGER_IOCP_CALLBACK_PORT",
        GetEnvPortOrDefault("MANAGER_IOCP_PORT", IOCP_CALLBACK_PORT_FALLBACK));
}

static std::string GetDirectoryName(const std::string& path)
{
    size_t pos1 = path.find_last_of('\\');
    size_t pos2 = path.find_last_of('/');

    size_t pos = std::string::npos;
    if (pos1 == std::string::npos)
    {
        pos = pos2;
    }
    else if (pos2 == std::string::npos)
    {
        pos = pos1;
    }
    else
    {
        pos = pos1 > pos2 ? pos1 : pos2;
    }

    if (pos == std::string::npos)
    {
        return std::string();
    }

    return path.substr(0, pos);
}

static std::string GetExecutableDirectory()
{
    char path[MAX_PATH]{};

    DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        return ".";
    }

    return GetDirectoryName(path);
}

static std::string MakeAbsoluteFromExeDir(const char* relativePath)
{
    std::string combined = GetExecutableDirectory();
    if (!combined.empty() && combined.back() != '\\' && combined.back() != '/')
    {
        combined += "\\";
    }
    combined += relativePath;

    char fullPath[MAX_PATH]{};
    DWORD len = GetFullPathNameA(combined.c_str(), MAX_PATH, fullPath, nullptr);
    if (len == 0 || len >= MAX_PATH)
    {
        return combined;
    }

    return fullPath;
}

static bool FileExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool DirectoryExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static std::string GetEnvironmentValue(const char* name)
{
    char* value = nullptr;
    size_t len = 0;

    if (_dupenv_s(&value, &len, name) != 0 || !value)
    {
        return std::string();
    }

    std::string result(value);
    free(value);
    return result;
}

static std::string GetParentDirectory(const std::string& path)
{
    if (path.empty())
    {
        return std::string();
    }

    return GetDirectoryName(path);
}

static std::string GetPackagedServerExePath()
{
    return GetEnvironmentValue(DEDI_PACKAGED_SERVER_ENV);
}

static std::string GetDediEditorExePath()
{
    std::string envPath = GetEnvironmentValue(DEDI_EDITOR_ENV);
    if (!envPath.empty())
    {
        return envPath;
    }

    if (FileExists(DEDI_EXE_DEFAULT_PATH))
    {
        return DEDI_EXE_DEFAULT_PATH;
    }

    std::string relativePath = MakeAbsoluteFromExeDir(DEDI_EXE_REL_PATH);
    if (FileExists(relativePath))
    {
        return relativePath;
    }

    return DEDI_EXE_FALLBACK_PATH;
}

static std::string GetDediProjectPath()
{
    std::string envPath = GetEnvironmentValue(DEDI_PROJECT_ENV);
    if (!envPath.empty())
    {
        return envPath;
    }

    if (FileExists(DEDI_PROJECT_DEFAULT_PATH))
    {
        return DEDI_PROJECT_DEFAULT_PATH;
    }

    return MakeAbsoluteFromExeDir(DEDI_PROJECT_REL_PATH);
}

static std::string GetDediWorkingDir()
{
    std::string envPath = GetEnvironmentValue(DEDI_WORKING_DIR_ENV);
    if (!envPath.empty())
    {
        return envPath;
    }

    if (DirectoryExists(DEDI_WORKING_DIR_DEFAULT_PATH))
    {
        return DEDI_WORKING_DIR_DEFAULT_PATH;
    }

    return MakeAbsoluteFromExeDir(DEDI_WORKING_DIR_REL_PATH);
}


// ===========================================================================
// Debug String Helpers
// ===========================================================================

const char* LoginResultToString(LoginResult r)
{
    switch (r)
    {
    case LoginResult::OK:                return "OK";
    case LoginResult::OK_RECONNECT:      return "OK_RECONNECT";
    case LoginResult::ID_NOT_FOUND:      return "ID_NOT_FOUND";
    case LoginResult::WRONG_PASSWORD:    return "WRONG_PASSWORD";
    case LoginResult::ALREADY_LOGGED_IN: return "ALREADY_LOGGED_IN";
    case LoginResult::ID_ALREADY_EXISTS: return "ID_ALREADY_EXISTS";
    case LoginResult::INVALID_FORMAT:    return "INVALID_FORMAT";
    default:                             return "UNKNOWN_LOGIN_RESULT";
    }
}

const char* RoomResultToString(RoomResult r)
{
    switch (r)
    {
    case RoomResult::OK:                return "OK";
    case RoomResult::INVALID_ROOM:      return "INVALID_ROOM";
    case RoomResult::FULL:              return "FULL";
    case RoomResult::IN_GAME:           return "IN_GAME";
    case RoomResult::ALREADY_IN_ROOM:   return "ALREADY_IN_ROOM";
    case RoomResult::NOT_IN_ROOM:       return "NOT_IN_ROOM";
    case RoomResult::BAD_PAYLOAD:       return "BAD_PAYLOAD";
    case RoomResult::TITLE_TOO_LONG:    return "TITLE_TOO_LONG";
    case RoomResult::NOT_HOST:          return "NOT_HOST";
    case RoomResult::NOT_ALL_READY:     return "NOT_ALL_READY";
    case RoomResult::NEED_MORE_PLAYERS: return "NEED_MORE_PLAYERS";
    default:                            return "UNKNOWN_ROOM_RESULT";
    }
}

const char* RoomStateToString(RoomState s)
{
    switch (s)
    {
    case RoomState::WAITING: return "WAITING";
    case RoomState::IN_GAME: return "IN_GAME";
    default:                 return "UNKNOWN_ROOM_STATE";
    }
}

extern void AddIO(struct ClientContext* c);
extern void ReleaseIO(struct ClientContext* c);


// ===========================================================================
// Constructor & Initialization
// ===========================================================================

LobbyService::LobbyService(NetApi& net)
    : m_net(net)
{
    InitializeSRWLock(&m_lock);
    InitPortPool(7777, 15);
}


// ===========================================================================
// Port Management
// ===========================================================================

void LobbyService::InitPortPool(uint16_t start, int count)
{
    m_freePorts.reserve(count);

    for (int i = count - 1; i >= 0; --i)
    {
        m_freePorts.push_back(start + i);
    }
}

uint16_t LobbyService::AllocPort()
{
    ReclaimQuarantinedPorts_Unsafe();

    if (m_freePorts.empty())
    {
        return 0;
    }

    uint16_t p = m_freePorts.back();
    m_freePorts.pop_back();
    return p;
}

void LobbyService::FreePort(uint16_t port)
{
    if (port == 0)
    {
        return;
    }

    if (std::find(m_freePorts.begin(), m_freePorts.end(), port) != m_freePorts.end())
    {
        LOBBY_WARN("[DEDI][WARN] FreePort skipped duplicate port=%u freeCount=%zu\n",
            port,
            m_freePorts.size());
        return;
    }

    m_freePorts.push_back(port);
}

bool LobbyService::IsUdpPortAvailable(uint16_t port) const
{
    SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (probe == INVALID_SOCKET)
    {
        return false;
    }

    const BOOL exclusiveAddressUse = TRUE;
    setsockopt(
        probe,
        SOL_SOCKET,
        SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusiveAddressUse),
        sizeof(exclusiveAddressUse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    const bool available =
        bind(probe, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR;
    closesocket(probe);
    return available;
}

void LobbyService::QuarantinePort_Unsafe(uint16_t port, const char* reason)
{
    if (port == 0)
    {
        return;
    }

    if (std::find(m_quarantinedPorts.begin(), m_quarantinedPorts.end(), port) ==
        m_quarantinedPorts.end())
    {
        m_quarantinedPorts.push_back(port);
    }

    LOBBY_WARN("[DEDI][WARN] Port quarantined port=%u reason=%s quarantined=%zu\n",
        port,
        reason ? reason : "Unknown",
        m_quarantinedPorts.size());
}

void LobbyService::ReclaimQuarantinedPorts_Unsafe()
{
    for (auto it = m_quarantinedPorts.begin(); it != m_quarantinedPorts.end();)
    {
        const uint16_t port = *it;
        if (!IsUdpPortAvailable(port))
        {
            ++it;
            continue;
        }

        it = m_quarantinedPorts.erase(it);
        FreePort(port);
        LOBBY_INFO("[DEDI] Quarantined port reclaimed port=%u remaining=%zu\n",
            port,
            m_quarantinedPorts.size());
    }
}

void LobbyService::CleanupDedicatedServerForRoom_Unsafe(Room& room, const char* reason, bool terminateProcess)
{
    const uint16_t oldPort = room.dedicatedPort;
    const HANDLE oldProcessHandle = room.dedicatedProcessHandle;
    const DWORD oldProcessId = room.dedicatedProcessId;

    room.dedicatedPort = 0;
    room.dedicatedProcessHandle = NULL;
    room.dedicatedProcessId = 0;
    room.dedicatedControlToken = 0;
    room.dedicatedReadyReceived = false;
    room.dedicatedLaunchTime = {};
    room.gameStartSent = false;
    room.handoverTickets.clear();
    room.ghostDisconnectTimes.clear();

    bool processExitConfirmed = oldProcessHandle == NULL;

    if (oldProcessHandle)
    {
        DWORD exitCode = STILL_ACTIVE;
        const bool hasExitCode = GetExitCodeProcess(oldProcessHandle, &exitCode) != FALSE;
        bool stillActive = hasExitCode && exitCode == STILL_ACTIVE;

        if (stillActive && terminateProcess)
        {
            if (TerminateProcess(oldProcessHandle, 0))
            {
                const DWORD waitResult = WaitForSingleObject(oldProcessHandle, DEDI_TERMINATE_WAIT_MS);
                processExitConfirmed = waitResult == WAIT_OBJECT_0;

                LOBBY_INFO("[DEDI] Cleanup roomId=%u reason=%s port=%u pid=%lu result=%s wait=%lu\n",
                    room.id,
                    reason ? reason : "Unknown",
                    oldPort,
                    oldProcessId,
                    processExitConfirmed ? "TERMINATED" : "TERMINATE_WAIT_TIMEOUT",
                    waitResult);
            }
            else
            {
                LOBBY_ERR("[DEDI][ERR] Cleanup roomId=%u reason=%s port=%u pid=%lu result=TERMINATE_FAILED gle=%lu\n",
                    room.id,
                    reason ? reason : "Unknown",
                    oldPort,
                    oldProcessId,
                    GetLastError());
            }
        }
        else if (!stillActive && hasExitCode)
        {
            processExitConfirmed = true;
            LOBBY_INFO("[DEDI] Cleanup roomId=%u reason=%s port=%u pid=%lu result=PROCESS_EXITED exitCode=%lu\n",
                room.id, reason ? reason : "Unknown", oldPort, oldProcessId, exitCode);
        }
        else
        {
            processExitConfirmed = false;
            LOBBY_WARN("[DEDI][WARN] Cleanup roomId=%u reason=%s port=%u pid=%lu result=EXIT_NOT_CONFIRMED\n",
                room.id, reason ? reason : "Unknown", oldPort, oldProcessId);
        }

        CloseHandle(oldProcessHandle);
    }

    if (oldPort != 0)
    {
        if (processExitConfirmed && IsUdpPortAvailable(oldPort))
        {
            FreePort(oldPort);
        }
        else
        {
            QuarantinePort_Unsafe(
                oldPort,
                processExitConfirmed ? "UdpPortStillBound" : "ProcessExitNotConfirmed");
        }
    }
}

void LobbyService::SweepDedicatedServerProcesses()
{
    std::vector<uint32_t> changedRooms;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);
    ReclaimQuarantinedPorts_Unsafe();

    for (auto& kv : m_rooms)
    {
        Room& room = kv.second;
        if (!room.dedicatedProcessHandle)
        {
            continue;
        }

        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(room.dedicatedProcessHandle, &exitCode))
        {
            LOBBY_ERR("[DEDI][ERR] Process handle invalid before cleanup. roomId=%u port=%u pid=%lu gle=%lu\n",
                room.id,
                room.dedicatedPort,
                room.dedicatedProcessId,
                GetLastError());

            CleanupDedicatedServerForRoom_Unsafe(room, "ProcessHandleInvalid", false);
            room.state = RoomState::WAITING;

            for (uint32_t sid : room.members)
            {
                room.readyStatus[sid] = (sid == room.hostId);
            }

            changedRooms.push_back(room.id);
            continue;
        }

        if (exitCode == STILL_ACTIVE)
        {
            if (!room.dedicatedReadyReceived &&
                room.dedicatedLaunchTime.time_since_epoch().count() != 0)
            {
                const auto elapsedSeconds =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        now - room.dedicatedLaunchTime).count();

                if (elapsedSeconds >= DEDI_READY_TIMEOUT_SECONDS)
                {
                    LOBBY_ERR("[DEDI][ERR] Ready timeout roomId=%u port=%u pid=%lu elapsed=%lld timeout=%d\n",
                        room.id,
                        room.dedicatedPort,
                        room.dedicatedProcessId,
                        static_cast<long long>(elapsedSeconds),
                        DEDI_READY_TIMEOUT_SECONDS);

                    CleanupDedicatedServerForRoom_Unsafe(room, "ReadyTimeout", true);
                    room.state = RoomState::WAITING;
                    for (uint32_t sid : room.members)
                    {
                        room.readyStatus[sid] = (sid == room.hostId);
                    }
                    changedRooms.push_back(room.id);
                }
            }
            continue;
        }

        LOBBY_ERR("[DEDI][ERR] Process exited before cleanup. roomId=%u port=%u pid=%lu exitCode=%lu\n",
            room.id,
            room.dedicatedPort,
            room.dedicatedProcessId,
            exitCode);

        CleanupDedicatedServerForRoom_Unsafe(room, "ProcessExited", false);
        room.state = RoomState::WAITING;

        for (uint32_t sid : room.members)
        {
            room.readyStatus[sid] = (sid == room.hostId);
        }

        changedRooms.push_back(room.id);
    }

    ReleaseSRWLockExclusive(&m_lock);

    for (uint32_t roomId : changedRooms)
    {
        BroadcastRoomList();
        BroadcastRoomMemberList(roomId);
    }
}

void LobbyService::SweepGhostSessions()
{
    std::vector<uint32_t> changedRooms;
    bool shouldBroadcastRoomList = false;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);

    for (auto itRoom = m_rooms.begin(); itRoom != m_rooms.end();)
    {
        Room& room = itRoom->second;
        if (room.state != RoomState::IN_GAME || room.ghostDisconnectTimes.empty())
        {
            ++itRoom;
            continue;
        }

        std::vector<uint32_t> expiredSids;
        for (const auto& ghost : room.ghostDisconnectTimes)
        {
            const uint32_t sid = ghost.first;
            if (m_ctxBySession.find(sid) != m_ctxBySession.end())
            {
                continue;
            }

            const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                now - ghost.second).count();
            if (elapsedSeconds >= IOCP_GHOST_RECONNECT_GRACE_SECONDS)
            {
                expiredSids.push_back(sid);
            }
        }

        if (expiredSids.empty())
        {
            ++itRoom;
            continue;
        }

        const uint32_t roomId = room.id;
        for (uint32_t sid : expiredSids)
        {
            room.ghostDisconnectTimes.erase(sid);
            room.members.erase(
                std::remove(room.members.begin(), room.members.end(), sid),
                room.members.end());
            room.readyStatus.erase(sid);
            room.handoverTickets.erase(sid);
            m_roomBySession.erase(sid);
            m_accountIdBySid.erase(sid);
            m_nicknameBySid.erase(sid);

            if (room.hostId == sid)
            {
                room.hostId = 0;
            }

            LOBBY_WARN("[LOBBY][WARN] GHOST_EXPIRED rid=%u sid=%u grace=%d\n",
                roomId,
                sid,
                IOCP_GHOST_RECONNECT_GRACE_SECONDS);
        }

        if (room.hostId == 0 && !room.members.empty())
        {
            room.hostId = room.members.front();
            room.readyStatus[room.hostId] = true;
            LOBBY_INFO("[ROOM] HOST_CHANGE rid=%u reason=GHOST_EXPIRED newHost=%u\n",
                roomId,
                room.hostId);
        }

        if (room.members.empty())
        {
            const uint16_t oldPort = room.dedicatedPort;
            const DWORD oldProcessId = room.dedicatedProcessId;

            if (room.dedicatedPort != 0 || room.dedicatedProcessHandle)
            {
                CleanupDedicatedServerForRoom_Unsafe(room, "GhostGraceAllExpired", true);
            }

            m_roomOrder.erase(
                std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                m_roomOrder.end());

            itRoom = m_rooms.erase(itRoom);
            shouldBroadcastRoomList = true;

            LOBBY_INFO("[ROOM] EMPTY_DELETE rid=%u reason=GHOST_GRACE_EXPIRED freePort=%u pid=%lu\n",
                roomId,
                oldPort,
                oldProcessId);
            continue;
        }

        changedRooms.push_back(roomId);
        shouldBroadcastRoomList = true;
        ++itRoom;
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (shouldBroadcastRoomList)
    {
        BroadcastRoomList();
    }

    for (uint32_t roomId : changedRooms)
    {
        BroadcastRoomMemberList(roomId);
    }
}

void LobbyService::TickMaintenance()
{
    SweepDedicatedServerProcesses();
    SweepGhostSessions();
}

uint32_t LobbyService::GenerateHandoverTicket_Unsafe(const Room& room) const
{
    static thread_local std::random_device randomDevice;
    static thread_local std::mt19937 rng(randomDevice());
    std::uniform_int_distribution<uint32_t> dist(100000000u, 0xFFFFFFFEu);

    uint32_t ticket = 0;
    do
    {
        ticket = dist(rng);
    }
    while (ticket == 0 ||
        std::find_if(
            room.handoverTickets.begin(),
            room.handoverTickets.end(),
            [ticket](const std::pair<const uint32_t, uint32_t>& entry)
            {
                return entry.second == ticket;
            }) != room.handoverTickets.end());

    return ticket;
}

uint64_t LobbyService::GenerateDedicatedControlToken_Unsafe() const
{
    static thread_local std::random_device randomDevice;
    static thread_local std::mt19937_64 rng(
        (static_cast<uint64_t>(randomDevice()) << 32) ^ randomDevice());

    uint64_t token = 0;
    while (token == 0)
    {
        token = rng();
    }
    return token;
}

std::string LobbyService::BuildHandoverTicketList_Unsafe(const Room& room) const
{
    std::string result;

    for (uint32_t sid : room.members)
    {
        auto itTicket = room.handoverTickets.find(sid);
        if (itTicket == room.handoverTickets.end())
        {
            continue;
        }

        if (!result.empty())
        {
            result += ",";
        }

        result += std::to_string(itTicket->second);
    }

    return result;
}


// ===========================================================================
// Dedicated Server Launch
// ===========================================================================

bool LobbyService::LaunchDedicatedServer(
    uint16_t port,
    uint32_t roomId,
    uint16_t requiredPlayers,
    const std::string& allowedTickets,
    uint64_t controlToken,
    uint32_t generation,
    HANDLE& outProcessHandle,
    DWORD& outProcessId)
{
    outProcessHandle = NULL;
    outProcessId = 0;

    const std::string iocpCallbackHost = GetIocpCallbackHost();
    const uint16_t iocpCallbackPort = GetIocpCallbackPort();

    char mapWithOptions[1024]{};
    sprintf_s(
        mapWithOptions,
        "%s?RoomId=%u?RequiredPlayers=%u?Tickets=%s?DediPort=%u?MatchGeneration=%u?ControlToken=%llu?IocpHost=%s?IocpPort=%u?ReconnectGraceSeconds=%d",
        DEDI_MAP_PATH,
        roomId,
        static_cast<unsigned>(requiredPlayers),
        allowedTickets.c_str(),
        static_cast<unsigned>(port),
        generation,
        static_cast<unsigned long long>(controlToken),
        iocpCallbackHost.c_str(),
        static_cast<unsigned>(iocpCallbackPort),
        IOCP_GHOST_RECONNECT_GRACE_SECONDS
    );

    const std::string packagedServerExePath = GetPackagedServerExePath();
    const bool bUsePackagedServer = !packagedServerExePath.empty();

    std::string workingDir;
    char cmdLine[2048]{};

    if (bUsePackagedServer)
    {
        workingDir = GetParentDirectory(packagedServerExePath);

        LOBBY_TRACE("[DEDI] ResolvePackagedServer serverExe=%s workingDir=%s serverExists=%d workingDirExists=%d\n",
            packagedServerExePath.c_str(),
            workingDir.c_str(),
            FileExists(packagedServerExePath) ? 1 : 0,
            DirectoryExists(workingDir) ? 1 : 0);

        sprintf_s(
            cmdLine,
            "\"%s\" \"%s\" -log -port=%u -Unattended",
            packagedServerExePath.c_str(),
            mapWithOptions,
            port
        );
    }
    else
    {
        const std::string editorExePath = GetDediEditorExePath();
        const std::string projectPath = GetDediProjectPath();
        workingDir = GetDediWorkingDir();

        LOBBY_TRACE("[DEDI] ResolveEditorServer editor=%s project=%s workingDir=%s editorExists=%d projectExists=%d workingDirExists=%d\n",
            editorExePath.c_str(),
            projectPath.c_str(),
            workingDir.c_str(),
            FileExists(editorExePath) ? 1 : 0,
            FileExists(projectPath) ? 1 : 0,
            DirectoryExists(workingDir) ? 1 : 0);

        sprintf_s(
            cmdLine,
            "\"%s\" \"%s\" \"%s\" -server -log -port=%u -NullRHI -NoLiveCoding -Unattended",
            editorExePath.c_str(),
            projectPath.c_str(),
            mapWithOptions,
            port
        );
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessA(
        nullptr,
        cmdLine,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        workingDir.c_str(),
        &si,
        &pi
    );

    if (!ok)
    {
        DWORD err = GetLastError();

        LOBBY_ERR("[DEDI][ERR] Launch failed. roomId=%u requiredPlayers=%u port=%u err=%lu usePackaged=%d\n",
            roomId,
            static_cast<unsigned>(requiredPlayers),
            port,
            err,
            bUsePackagedServer ? 1 : 0);

        return false;
    }

    LOBBY_INFO("[DEDI] Launch success. roomId=%u requiredPlayers=%u port=%u pid=%lu usePackaged=%d\n",
        roomId,
        static_cast<unsigned>(requiredPlayers),
        port,
        pi.dwProcessId,
        bUsePackagedServer ? 1 : 0);

    CloseHandle(pi.hThread);
    outProcessHandle = pi.hProcess;
    outProcessId = pi.dwProcessId;

    return true;
}


// ===========================================================================
// Connection Event Handlers
// ===========================================================================

void LobbyService::OnClientAccepted(ClientContext* c)
{
    AcquireSRWLockExclusive(&m_lock);

    uint32_t sid = m_nextSessionId++;

    m_sessionByCtx[c] = sid;
    m_ctxBySession[sid] = c;
    m_roomBySession[sid] = 0;

    LOBBY_TRACE("[LOBBY] ACCEPT sid=%u\n", sid);

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendWelcome(c, sid);
}

void LobbyService::OnClientDisconnected(ClientContext* c)
{
    bool shouldBroadcast = false;
    uint32_t oldRid = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto it = m_sessionByCtx.find(c);
    if (it != m_sessionByCtx.end())
    {
        uint32_t sid = it->second;

        auto itRoomBySession = m_roomBySession.find(sid);
        if (itRoomBySession != m_roomBySession.end())
        {
            oldRid = itRoomBySession->second;
        }

        LOBBY_TRACE("[LOBBY] DISCONNECT sid=%u rid=%u\n", sid, oldRid);

        if (oldRid != 0 && m_rooms.count(oldRid) > 0)
        {
            Room& r = m_rooms[oldRid];

            if (r.state != RoomState::IN_GAME)
            {
                LOBBY_TRACE("[LOBBY] DISCONNECT sid=%u leave room internally\n", sid);
                LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
            }
            else
            {
                r.ghostDisconnectTimes[sid] = std::chrono::steady_clock::now();
                LOBBY_TRACE("[LOBBY] DISCONNECT sid=%u stays as ghost (IN_GAME)\n", sid);
            }
        }

        m_ctxBySession.erase(sid);
        m_sessionByCtx.erase(it);

        // m_accountIdBySid, m_nicknameBySid are intentionally kept for reconnect.
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (shouldBroadcast)
    {
        BroadcastRoomList();

        if (oldRid != 0)
        {
            BroadcastRoomMemberList(oldRid);
        }
    }
}


// ===========================================================================
// Packet Dispatcher
// ===========================================================================

void LobbyService::OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen)
{
    PacketType pktType = static_cast<PacketType>(type);

    bool allowedWithoutLogin =
        (pktType == PacketType::C2S_LOGIN_REQ) ||
        (pktType == PacketType::C2S_REGISTER_REQ) ||
        (pktType == PacketType::C2S_PING) ||
        (pktType == PacketType::D2L_MATCH_END_NOTIFY) ||
        (pktType == PacketType::D2L_SERVER_READY_NOTIFY);

    bool isLoggedIn = false;

    {
        AcquireSRWLockShared(&m_lock);

        auto itSession = m_sessionByCtx.find(c);
        if (itSession != m_sessionByCtx.end())
        {
            uint32_t sid = itSession->second;
            isLoggedIn = (m_accountIdBySid.find(sid) != m_accountIdBySid.end());
        }

        ReleaseSRWLockShared(&m_lock);
    }

    if (!isLoggedIn && !allowedWithoutLogin)
    {
        return;
    }

    switch (pktType)
    {
    case PacketType::D2L_MATCH_END_NOTIFY:
        HandleDediMatchEndNotify(c, payload, payloadLen);
        break;

    case PacketType::D2L_SERVER_READY_NOTIFY:
        HandleDediServerReadyNotify(c, payload, payloadLen);
        break;

    case PacketType::C2S_LOGIN_REQ:
        HandleLoginReq(c, payload, payloadLen);
        break;

    case PacketType::C2S_REGISTER_REQ:
        HandleRegisterReq(c, payload, payloadLen);
        break;

    case PacketType::C2S_ROOM_LIST_REQ:
        HandleRoomListReq(c);
        break;
    case PacketType::C2S_ROOM_CREATE_REQ:
        HandleRoomCreateReq(c, payload, payloadLen);
        break;

    case PacketType::C2S_ROOM_JOIN_REQ:
        HandleRoomJoinReq(c, payload, payloadLen);
        break;

    case PacketType::C2S_ROOM_LEAVE_REQ:
        HandleRoomLeaveReq(c);
        break;

    case PacketType::C2S_ROOM_READY_REQ:
        HandleRoomReadyReq(c, payload, payloadLen);
        break;

    case PacketType::C2S_ROOM_START_REQ:
        HandleRoomStartReq(c);
        break;

    case PacketType::C2S_PING:
        m_net.SendPong(c);
        break;

    default:
        break;
    }
}


// ===========================================================================
// Auth Logic
// ===========================================================================

bool LobbyService::ParseAuthPayload(
    const char* payload,
    uint16_t payloadLen,
    std::string& outId,
    std::string& outPw,
    std::string* outNickname
)
{
    if (!payload || payloadLen < 1)
    {
        return false;
    }

    uint8_t idLen = static_cast<uint8_t>(payload[0]);

    if (idLen == 0 || idLen > MAX_ID_LEN)
    {
        return false;
    }

    if (payloadLen < static_cast<uint16_t>(1 + idLen + 1))
    {
        return false;
    }

    outId.assign(payload + 1, idLen);

    uint8_t pwLen = static_cast<uint8_t>(payload[1 + idLen]);

    if (pwLen == 0 || pwLen > MAX_PW_LEN)
    {
        return false;
    }

    if (payloadLen < static_cast<uint16_t>(1 + idLen + 1 + pwLen))
    {
        return false;
    }

    outPw.assign(payload + 1 + idLen + 1, pwLen);

    if (outNickname)
    {
        size_t nickOffset = static_cast<size_t>(1 + idLen + 1 + pwLen);

        if (payloadLen > nickOffset)
        {
            if (payloadLen < nickOffset + 1)
            {
                return false;
            }

            uint8_t nickLen = static_cast<uint8_t>(payload[nickOffset]);

            if (nickLen == 0 || nickLen > MAX_NICKNAME_LEN)
            {
                return false;
            }

            if (payloadLen < nickOffset + 1 + nickLen)
            {
                return false;
            }

            outNickname->assign(payload + nickOffset + 1, nickLen);
        }
        else
        {
            *outNickname = outId;
        }
    }

    return true;
}

void LobbyService::HandleRegisterReq(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    std::string id;
    std::string pw;
    std::string nickname;

    if (!ParseAuthPayload(payload, payloadLen, id, pw, &nickname))
    {
        printf("[AUTH] REGISTER result=%s\n",
            LoginResultToString(LoginResult::INVALID_FORMAT));

        m_net.SendRegisterRes(c, LoginResult::INVALID_FORMAT);
        return;
    }

    if (nickname.empty())
    {
        nickname = id;
    }

    AcquireSRWLockExclusive(&m_lock);

    if (m_userDB.find(id) != m_userDB.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[AUTH] REGISTER id=%s result=%s\n",
            id.c_str(),
            LoginResultToString(LoginResult::ID_ALREADY_EXISTS));

        m_net.SendRegisterRes(c, LoginResult::ID_ALREADY_EXISTS);
        return;
    }

    UserRecord record;
    record.password = pw;
    record.nickname = nickname;

    m_userDB[id] = record;

    ReleaseSRWLockExclusive(&m_lock);

    printf("[AUTH] REGISTER id=%s nickname=%s result=%s\n",
        id.c_str(),
        nickname.c_str(),
        LoginResultToString(LoginResult::OK));

    m_net.SendRegisterRes(c, LoginResult::OK);
}

void LobbyService::HandleLoginReq(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    std::string id;
    std::string pw;

    if (!ParseAuthPayload(payload, payloadLen, id, pw))
    {
        printf("[AUTH] LOGIN result=%s\n",
            LoginResultToString(LoginResult::INVALID_FORMAT));

        m_net.SendLoginRes(c, LoginResult::INVALID_FORMAT);
        return;
    }

    AcquireSRWLockExclusive(&m_lock);

    auto itUser = m_userDB.find(id);
    if (itUser == m_userDB.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[AUTH] LOGIN id=%s result=%s\n",
            id.c_str(),
            LoginResultToString(LoginResult::ID_NOT_FOUND));

        m_net.SendLoginRes(c, LoginResult::ID_NOT_FOUND);
        return;
    }

    if (itUser->second.password != pw)
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[AUTH] LOGIN id=%s result=%s\n",
            id.c_str(),
            LoginResultToString(LoginResult::WRONG_PASSWORD));

        m_net.SendLoginRes(c, LoginResult::WRONG_PASSWORD);
        return;
    }

    uint32_t oldSid = 0;

    for (const auto& kv : m_accountIdBySid)
    {
        if (kv.second == id)
        {
            oldSid = kv.first;
            break;
        }
    }

    if (oldSid != 0)
    {
        if (m_ctxBySession.count(oldSid) > 0)
        {
            ReleaseSRWLockExclusive(&m_lock);

            printf("[AUTH] LOGIN id=%s result=%s\n",
                id.c_str(),
                LoginResultToString(LoginResult::ALREADY_LOGGED_IN));

            m_net.SendLoginRes(c, LoginResult::ALREADY_LOGGED_IN);
            return;
        }

        auto itCurrentSid = m_sessionByCtx.find(c);
        if (itCurrentSid == m_sessionByCtx.end())
        {
            ReleaseSRWLockExclusive(&m_lock);

            printf("[AUTH] LOGIN id=%s result=%s (no session)\n",
                id.c_str(),
                LoginResultToString(LoginResult::INVALID_FORMAT));

            m_net.SendLoginRes(c, LoginResult::INVALID_FORMAT);
            return;
        }

        uint32_t currentSid = itCurrentSid->second;

        m_ctxBySession.erase(currentSid);
        m_roomBySession.erase(currentSid);
        m_accountIdBySid.erase(currentSid);
        m_nicknameBySid.erase(currentSid);

        m_sessionByCtx[c] = oldSid;
        m_ctxBySession[oldSid] = c;
        m_nicknameBySid[oldSid] = itUser->second.nickname;

        uint32_t rid = 0;
        auto itRoomBySession = m_roomBySession.find(oldSid);
        if (itRoomBySession != m_roomBySession.end())
        {
            rid = itRoomBySession->second;
        }

        if (rid != 0 && m_rooms.count(rid) > 0)
        {
            Room& r = m_rooms[rid];

            if (r.state == RoomState::IN_GAME && r.gameStartSent)
            {
                uint16_t port = r.dedicatedPort;
                uint32_t reconnectTicket = 0;
                auto itTicket = r.handoverTickets.find(oldSid);
                if (itTicket != r.handoverTickets.end())
                {
                    reconnectTicket = itTicket->second;
                }

                if (reconnectTicket == 0)
                {
                    ReleaseSRWLockExclusive(&m_lock);

                    printf("[AUTH] LOGIN id=%s sid=%u result=%s reason=NO_HANDOVER_TICKET\n",
                        id.c_str(),
                        oldSid,
                        LoginResultToString(LoginResult::INVALID_FORMAT));

                    m_net.SendLoginRes(c, LoginResult::INVALID_FORMAT);
                    return;
                }

                r.ghostDisconnectTimes.erase(oldSid);

                ReleaseSRWLockExclusive(&m_lock);

                printf("[AUTH] LOGIN id=%s sid=%u result=%s port=%u\n",
                    id.c_str(),
                    oldSid,
                    LoginResultToString(LoginResult::OK_RECONNECT),
                    port);

                m_net.SendLoginRes(c, LoginResult::OK_RECONNECT);
                const std::string dedicatedHost = GetDediAdvertisedHost();
                m_net.SendGameStart(c, dedicatedHost.c_str(), port, reconnectTicket);
                return;
            }
        }
    }
    else
    {
        auto itCurrentSid = m_sessionByCtx.find(c);
        if (itCurrentSid == m_sessionByCtx.end())
        {
            ReleaseSRWLockExclusive(&m_lock);

            m_net.SendLoginRes(c, LoginResult::INVALID_FORMAT);
            return;
        }

        uint32_t currentSid = itCurrentSid->second;

        m_accountIdBySid[currentSid] = id;
        m_nicknameBySid[currentSid] = itUser->second.nickname;
    }

    uint32_t loginSid = m_sessionByCtx[c];

    ReleaseSRWLockExclusive(&m_lock);

    printf("[AUTH] LOGIN id=%s sid=%u result=%s\n",
        id.c_str(),
        loginSid,
        LoginResultToString(LoginResult::OK));

    m_net.SendLoginRes(c, LoginResult::OK);
    HandleRoomListReq(c);
}


// ===========================================================================
// Room Logic Handlers
// ===========================================================================

RoomResult LobbyService::HandleRoomListReq(ClientContext* c)
{
    AcquireSRWLockShared(&m_lock);

    auto views = BuildRoomListView_Unsafe();

    ReleaseSRWLockShared(&m_lock);

    m_net.SendRoomListRes(c, views);
    return RoomResult::OK;
}

RoomResult LobbyService::HandleRoomCreateReq(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    if (!payload || payloadLen < 1)
    {
        printf("[ROOM] CREATE result=%s\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));

        m_net.SendRoomCreateRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    uint8_t titleLen = static_cast<uint8_t>(payload[0]);

    if (titleLen > ROOM_TITLE_MAX || payloadLen < static_cast<uint16_t>(1 + titleLen))
    {
        printf("[ROOM] CREATE result=%s\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));

        m_net.SendRoomCreateRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        m_net.SendRoomCreateRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    uint32_t sid = itSession->second;

    if (m_roomBySession[sid] != 0)
    {
        uint32_t currentRid = m_roomBySession[sid];

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] CREATE sid=%u result=%s currentRid=%u\n",
            sid,
            RoomResultToString(RoomResult::ALREADY_IN_ROOM),
            currentRid);

        m_net.SendRoomCreateRes(c, RoomResult::ALREADY_IN_ROOM, nullptr);
        return RoomResult::ALREADY_IN_ROOM;
    }

    Room r;
    r.id = m_nextRoomId++;
    r.title.assign(payload + 1, titleLen);

    r.members.push_back(sid);
    r.hostId = sid;
    r.readyStatus[sid] = true;

    uint32_t rid = r.id;

    m_rooms[rid] = std::move(r);
    m_roomOrder.push_back(rid);
    m_roomBySession[sid] = rid;

    RoomInfoView view = BuildRoomView_Unsafe(m_rooms[rid]);
    std::string createdTitle = m_rooms[rid].title;
    uint32_t hostSid = m_rooms[rid].hostId;

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] CREATE sid=%u rid=%u title=\"%s\" host=%u result=%s\n",
        sid,
        rid,
        createdTitle.c_str(),
        hostSid,
        RoomResultToString(RoomResult::OK));

    m_net.SendRoomCreateRes(c, RoomResult::OK, &view);

    BroadcastRoomList();
    BroadcastRoomMemberList(rid);

    return RoomResult::OK;
}

RoomResult LobbyService::HandleRoomJoinReq(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    uint32_t rid = 0;
    uint32_t sid = 0;

    RoomResult result = RoomResult::OK;
    RoomInfoView view{};

    bool hasView = false;
    bool shouldBroadcast = false;

    if (!ReadU32(payload, payloadLen, rid))
    {
        printf("[ROOM] JOIN result=%s (bad payload)\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));

        m_net.SendRoomJoinRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        m_net.SendRoomJoinRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    sid = itSession->second;

    auto itMyRoom = m_roomBySession.find(sid);
    if (itMyRoom != m_roomBySession.end() && itMyRoom->second != 0)
    {
        result = RoomResult::ALREADY_IN_ROOM;
    }
    else
    {
        auto itRoom = m_rooms.find(rid);
        if (itRoom == m_rooms.end())
        {
            result = RoomResult::INVALID_ROOM;
        }
        else
        {
            Room& r = itRoom->second;

            if (r.state == RoomState::IN_GAME)
            {
                result = RoomResult::IN_GAME;
            }
            else if (r.members.size() >= ROOM_MAX_PLAYERS)
            {
                result = RoomResult::FULL;
            }
            else
            {
                r.members.push_back(sid);
                r.readyStatus[sid] = false;
                m_roomBySession[sid] = rid;

                view = BuildRoomView_Unsafe(r);
                hasView = true;
                shouldBroadcast = true;
                result = RoomResult::OK;
            }
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] JOIN sid=%u targetRid=%u result=%s",
        sid,
        rid,
        RoomResultToString(result));

    if (hasView)
    {
        printf(" roomState=%s players=%u/%u host=%u title=\"%.*s\"",
            RoomStateToString(view.state),
            view.curPlayers,
            view.maxPlayers,
            view.hostId,
            view.titleLen,
            view.title);
    }

    printf("\n");

    m_net.SendRoomJoinRes(c, result, hasView ? &view : nullptr);

    if (shouldBroadcast)
    {
        BroadcastRoomList();
        BroadcastRoomMemberList(rid);
    }

    return result;
}

RoomResult LobbyService::HandleRoomLeaveReq(ClientContext* c)
{
    RoomResult result = RoomResult::BAD_PAYLOAD;
    bool shouldBroadcast = false;

    uint32_t sid = 0;
    uint32_t oldRid = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession != m_sessionByCtx.end())
    {
        sid = itSession->second;

        auto itRoomBySession = m_roomBySession.find(sid);
        if (itRoomBySession != m_roomBySession.end())
        {
            oldRid = itRoomBySession->second;
        }

        result = LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
    }

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] LEAVE sid=%u rid=%u result=%s\n",
        sid,
        oldRid,
        RoomResultToString(result));

    if (result == RoomResult::OK)
    {
        m_net.SendRoomLeaveRes(c, RoomResult::OK);
    }

    if (shouldBroadcast)
    {
        BroadcastRoomList();

        if (oldRid != 0)
        {
            BroadcastRoomMemberList(oldRid);
        }
    }

    return result;
}


// ===========================================================================
// Room Actions
// ===========================================================================

void LobbyService::HandleRoomReadyReq(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    if (!payload || payloadLen < 1)
    {
        return;
    }

    bool isReady = (payload[0] != 0);

    uint32_t sid = 0;
    uint32_t rid = 0;
    std::vector<uint32_t> membersCopy;

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end())
    {
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    sid = itSession->second;

    auto itRoomBySession = m_roomBySession.find(sid);
    if (itRoomBySession == m_roomBySession.end())
    {
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    rid = itRoomBySession->second;

    if (rid == 0)
    {
        printf("[ROOM] READY sid=%u ignored (NOT_IN_ROOM)\n", sid);
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    Room& r = itRoom->second;

    if (r.state == RoomState::IN_GAME || r.hostId == sid)
    {
        printf("[ROOM] READY sid=%u rid=%u ignored (IN_GAME or HOST)\n", sid, rid);
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    r.readyStatus[sid] = isReady;
    membersCopy = r.members;

    printf("[ROOM] READY sid=%u rid=%u value=%d\n",
        sid,
        rid,
        isReady ? 1 : 0);

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] READY_BROADCAST rid=%u members=%zu fromSid=%u value=%d\n",
        rid,
        membersCopy.size(),
        sid,
        isReady ? 1 : 0);

    AcquireSRWLockShared(&m_lock);

    for (uint32_t mSid : membersCopy)
    {
        auto itCtx = m_ctxBySession.find(mSid);
        if (itCtx != m_ctxBySession.end())
        {
            m_net.SendRoomReadyBrd(itCtx->second, sid, isReady);
        }
    }

    ReleaseSRWLockShared(&m_lock);

    BroadcastRoomMemberList(rid);
}

void LobbyService::HandleRoomStartReq(ClientContext* c)
{
    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START result=%s (no session)\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

    uint32_t sid = itSession->second;

    auto itRoomBySession = m_roomBySession.find(sid);
    if (itRoomBySession == m_roomBySession.end() || itRoomBySession->second == 0)
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u result=%s\n",
            sid,
            RoomResultToString(RoomResult::NOT_IN_ROOM));

        m_net.SendRoomStartRes(c, RoomResult::NOT_IN_ROOM);
        return;
    }

    uint32_t rid = itRoomBySession->second;

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid,
            rid,
            RoomResultToString(RoomResult::INVALID_ROOM));

        m_net.SendRoomStartRes(c, RoomResult::INVALID_ROOM);
        return;
    }

    Room& r = itRoom->second;

    if (r.state == RoomState::IN_GAME)
    {
        uint16_t existingPort = r.dedicatedPort;

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u ignored result=%s existingPort=%u\n",
            sid,
            rid,
            RoomResultToString(RoomResult::IN_GAME),
            existingPort);

        m_net.SendRoomStartRes(c, RoomResult::IN_GAME);
        return;
    }

    if (r.hostId != sid)
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid,
            rid,
            RoomResultToString(RoomResult::NOT_HOST));

        m_net.SendRoomStartRes(c, RoomResult::NOT_HOST);
        return;
    }

    if (r.members.size() < MIN_PLAYERS_TO_START)
    {
        size_t memberCount = r.members.size();

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u players=%zu result=%s\n",
            sid,
            rid,
            memberCount,
            RoomResultToString(RoomResult::NEED_MORE_PLAYERS));

        m_net.SendRoomStartRes(c, RoomResult::NEED_MORE_PLAYERS);
        return;
    }

    for (uint32_t mSid : r.members)
    {
        if (mSid != r.hostId && !r.readyStatus[mSid])
        {
            ReleaseSRWLockExclusive(&m_lock);

            printf("[ROOM] START sid=%u rid=%u blockerSid=%u result=%s\n",
                sid,
                rid,
                mSid,
                RoomResultToString(RoomResult::NOT_ALL_READY));

            m_net.SendRoomStartRes(c, RoomResult::NOT_ALL_READY);
            return;
        }
    }

    uint16_t port = AllocPort();
    if (port == 0)
    {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=NO_FREE_PORT\n",
            sid,
            rid);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

    std::vector<uint32_t> membersCopy = r.members;
    r.handoverTickets.clear();
    r.ghostDisconnectTimes.clear();
    for (uint32_t mSid : r.members)
    {
        const uint32_t ticket = GenerateHandoverTicket_Unsafe(r);
        r.handoverTickets[mSid] = ticket;
    }

    const std::string allowedTickets = BuildHandoverTicketList_Unsafe(r);
    if (allowedTickets.empty())
    {
        FreePort(port);
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=NO_HANDOVER_TICKETS\n",
            sid,
            rid);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

    // Important:
    // Reserve the room as IN_GAME before launching Dedi.
    // This prevents duplicate Start requests from launching 7777, 7778, ...
    r.state = RoomState::IN_GAME;
    r.dedicatedPort = port;
    r.gameStartSent = false;
    r.dedicatedGeneration++;
    if (r.dedicatedGeneration == 0)
    {
        r.dedicatedGeneration = 1;
    }
    r.dedicatedControlToken = GenerateDedicatedControlToken_Unsafe();
    r.dedicatedReadyReceived = false;
    r.dedicatedLaunchTime = std::chrono::steady_clock::now();

    const uint32_t dedicatedGeneration = r.dedicatedGeneration;
    const uint64_t dedicatedControlToken = r.dedicatedControlToken;

    ReleaseSRWLockExclusive(&m_lock);

    uint16_t requiredPlayers = static_cast<uint16_t>(membersCopy.size());
    HANDLE dediProcessHandle = NULL;
    DWORD dediProcessId = 0;

    if (!LaunchDedicatedServer(
        port,
        rid,
        requiredPlayers,
        allowedTickets,
        dedicatedControlToken,
        dedicatedGeneration,
        dediProcessHandle,
        dediProcessId))
    {
        AcquireSRWLockExclusive(&m_lock);

        auto itRollbackRoom = m_rooms.find(rid);
        if (itRollbackRoom != m_rooms.end())
        {
            itRollbackRoom->second.state = RoomState::WAITING;
            itRollbackRoom->second.dedicatedPort = 0;
            itRollbackRoom->second.dedicatedProcessHandle = NULL;
            itRollbackRoom->second.dedicatedProcessId = 0;
            itRollbackRoom->second.dedicatedControlToken = 0;
            itRollbackRoom->second.dedicatedReadyReceived = false;
            itRollbackRoom->second.dedicatedLaunchTime = {};
            itRollbackRoom->second.gameStartSent = false;
            itRollbackRoom->second.handoverTickets.clear();
            itRollbackRoom->second.ghostDisconnectTimes.clear();
        }

        FreePort(port);

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=DEDI_LAUNCH_FAILED port=%u\n",
            sid,
            rid,
            port);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        BroadcastRoomList();
        BroadcastRoomMemberList(rid);
        return;
    }

    bool launchOwnedByRoom = false;
    const DWORD launchedDediProcessId = dediProcessId;
    AcquireSRWLockExclusive(&m_lock);

    auto itStoreRoom = m_rooms.find(rid);
    if (itStoreRoom != m_rooms.end() &&
        itStoreRoom->second.state == RoomState::IN_GAME &&
        itStoreRoom->second.dedicatedPort == port &&
        itStoreRoom->second.dedicatedGeneration == dedicatedGeneration &&
        itStoreRoom->second.dedicatedControlToken == dedicatedControlToken)
    {
        itStoreRoom->second.dedicatedProcessHandle = dediProcessHandle;
        itStoreRoom->second.dedicatedProcessId = dediProcessId;
        dediProcessHandle = NULL;
        dediProcessId = 0;
        launchOwnedByRoom = true;
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (!launchOwnedByRoom)
    {
        if (dediProcessHandle)
        {
            TerminateProcess(dediProcessHandle, 0);
            CloseHandle(dediProcessHandle);
        }

        LOBBY_WARN("[ROOM][WARN] START sid=%u rid=%u result=ROOM_RELEASED_DURING_LAUNCH port=%u\n",
            sid,
            rid,
            port);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        BroadcastRoomList();
        BroadcastRoomMemberList(rid);
        return;
    }

    LOBBY_INFO("[ROOM] START sid=%u rid=%u result=%s port=%u pid=%lu members=%zu\n",
        sid,
        rid,
        RoomResultToString(RoomResult::OK),
        port,
        launchedDediProcessId,
        membersCopy.size());

    m_net.SendRoomStartRes(c, RoomResult::OK);

    LOBBY_INFO("[ROOM] GAME_START_WAIT_READY rid=%u port=%u members=%zu\n",
        rid,
        port,
        membersCopy.size());

    BroadcastRoomList();
    BroadcastRoomMemberList(rid);
}

void LobbyService::SendGameStartForRoom_Unsafe(uint32_t roomId, const char* reason)
{
    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        LOBBY_WARN("[ROOM][WARN] GAME_START_SKIP rid=%u reason=%s result=ROOM_NOT_FOUND\n",
            roomId,
            reason ? reason : "Unknown");
        return;
    }

    Room& room = itRoom->second;
    if (room.state != RoomState::IN_GAME || room.dedicatedPort == 0)
    {
        LOBBY_WARN("[ROOM][WARN] GAME_START_SKIP rid=%u reason=%s state=%s port=%u result=ROOM_NOT_IN_GAME\n",
            roomId,
            reason ? reason : "Unknown",
            RoomStateToString(room.state),
            room.dedicatedPort);
        return;
    }

    if (room.gameStartSent)
    {
        LOBBY_TRACE("[ROOM] GAME_START_SKIP rid=%u reason=%s port=%u result=ALREADY_SENT\n",
            roomId,
            reason ? reason : "Unknown",
            room.dedicatedPort);
        return;
    }

    room.gameStartSent = true;
    const uint16_t port = room.dedicatedPort;
    std::vector<uint32_t> membersCopy = room.members;
    const std::string dedicatedHost = GetDediAdvertisedHost();

    for (uint32_t mSid : membersCopy)
    {
        auto itCtx = m_ctxBySession.find(mSid);
        if (itCtx == m_ctxBySession.end() || itCtx->second == nullptr)
        {
            LOBBY_WARN("[ROOM][WARN] GAME_START_TARGET_OFFLINE rid=%u sid=%u reason=%s\n",
                roomId,
                mSid,
                reason ? reason : "Unknown");
            continue;
        }

        auto itTicket = room.handoverTickets.find(mSid);
        if (itTicket == room.handoverTickets.end() || itTicket->second == 0)
        {
            LOBBY_WARN("[ROOM][WARN] GAME_START_TARGET_NO_TICKET rid=%u sid=%u reason=%s\n",
                roomId,
                mSid,
                reason ? reason : "Unknown");
            continue;
        }

        LOBBY_INFO("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u reason=%s ticketIssued=1\n",
            roomId,
            mSid,
            dedicatedHost.c_str(),
            port,
            reason ? reason : "Unknown");

        m_net.SendGameStart(itCtx->second, dedicatedHost.c_str(), port, itTicket->second);
    }
}


// ===========================================================================
// Helper Functions
// ===========================================================================

RoomInfoView LobbyService::BuildRoomView_Unsafe(const Room& room) const
{
    RoomInfoView v{};

    v.roomId = room.id;
    v.state = room.state;
    v.curPlayers = static_cast<uint8_t>(room.members.size());
    v.maxPlayers = ROOM_MAX_PLAYERS;
    v.hostId = room.hostId;

    v.titleLen = static_cast<uint8_t>((std::min)(
        static_cast<size_t>(ROOM_TITLE_MAX),
        room.title.size()
        ));

    if (v.titleLen > 0)
    {
        std::memcpy(v.title, room.title.data(), v.titleLen);
    }

    return v;
}

std::vector<RoomInfoView> LobbyService::BuildRoomListView_Unsafe() const
{
    std::vector<RoomInfoView> views;

    for (uint32_t rid : m_roomOrder)
    {
        auto itRoom = m_rooms.find(rid);
        if (itRoom != m_rooms.end())
        {
            views.push_back(BuildRoomView_Unsafe(itRoom->second));
        }
    }

    return views;
}

std::vector<RoomMemberInfoView> LobbyService::BuildRoomMemberListView_Unsafe(const Room& room) const
{
    std::vector<RoomMemberInfoView> views;
    views.reserve(room.members.size());

    for (uint32_t sid : room.members)
    {
        RoomMemberInfoView v{};

        v.sessionId = sid;
        v.isHost = (sid == room.hostId) ? 1 : 0;

        auto itReady = room.readyStatus.find(sid);
        v.isReady = (itReady != room.readyStatus.end() && itReady->second) ? 1 : 0;

        std::string nickname;

        auto itNick = m_nicknameBySid.find(sid);
        if (itNick != m_nicknameBySid.end())
        {
            nickname = itNick->second;
        }
        else
        {
            auto itAccount = m_accountIdBySid.find(sid);
            if (itAccount != m_accountIdBySid.end())
            {
                nickname = itAccount->second;
            }
            else
            {
                nickname = "Player" + std::to_string(sid);
            }
        }

        v.nicknameLen = static_cast<uint8_t>((std::min)(
            nickname.size(),
            static_cast<size_t>(MAX_NICKNAME_LEN)
            ));

        if (v.nicknameLen > 0)
        {
            std::memcpy(v.nickname, nickname.data(), v.nicknameLen);
        }

        views.push_back(v);
    }

    return views;
}

RoomResult LobbyService::LeaveRoomInternal_Unsafe(uint32_t sid, bool& shouldBroadcast)
{
    shouldBroadcast = false;

    auto itRoomBySession = m_roomBySession.find(sid);
    if (itRoomBySession == m_roomBySession.end())
    {
        return RoomResult::BAD_PAYLOAD;
    }

    uint32_t rid = itRoomBySession->second;
    if (rid == 0)
    {
        return RoomResult::NOT_IN_ROOM;
    }

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end())
    {
        m_roomBySession[sid] = 0;
        return RoomResult::INVALID_ROOM;
    }

    Room& r = itRoom->second;

    for (auto it = r.members.begin(); it != r.members.end(); ++it)
    {
        if (*it == sid)
        {
            r.members.erase(it);
            break;
        }
    }

    m_roomBySession[sid] = 0;
    r.readyStatus.erase(sid);
    r.handoverTickets.erase(sid);
    r.ghostDisconnectTimes.erase(sid);

    if (r.hostId == sid)
    {
        if (!r.members.empty())
        {
            uint32_t oldHost = sid;
            r.hostId = r.members.front();
            r.readyStatus[r.hostId] = true;

            LOBBY_INFO("[ROOM] HOST_CHANGE rid=%u oldHost=%u newHost=%u\n",
                rid,
                oldHost,
                r.hostId);
        }
        else
        {
            r.hostId = 0;
        }
    }

    if (r.members.empty())
    {
        uint16_t oldPort = r.dedicatedPort;
        DWORD oldProcessId = r.dedicatedProcessId;

        if (r.dedicatedPort != 0 || r.dedicatedProcessHandle)
        {
            CleanupDedicatedServerForRoom_Unsafe(r, "RoomEmpty", true);
        }

        m_roomOrder.erase(
            std::remove(m_roomOrder.begin(), m_roomOrder.end(), rid),
            m_roomOrder.end());

        m_rooms.erase(itRoom);

        LOBBY_INFO("[ROOM] EMPTY_DELETE rid=%u freePort=%u pid=%lu\n",
            rid,
            oldPort,
            oldProcessId);

        shouldBroadcast = true;
        return RoomResult::OK;
    }

    shouldBroadcast = true;
    return RoomResult::OK;
}

void LobbyService::BroadcastRoomList()
{
    std::vector<RoomInfoView> rooms;
    std::vector<ClientContext*> targets;

    AcquireSRWLockShared(&m_lock);

    rooms = BuildRoomListView_Unsafe();

    for (const auto& kv : m_sessionByCtx)
    {
        ClientContext* ctx = kv.first;
        uint32_t sid = kv.second;

        if (m_accountIdBySid.find(sid) == m_accountIdBySid.end())
        {
            continue;
        }

        LOBBY_TRACE("[ROOM] BROADCAST_TARGET sid=%u\n", sid);

        targets.push_back(ctx);
        AddIO(ctx);
    }

    ReleaseSRWLockShared(&m_lock);

    LOBBY_TRACE("[ROOM] BROADCAST_ROOM_LIST targets=%zu rooms=%zu\n",
        targets.size(),
        rooms.size());

    for (ClientContext* ctx : targets)
    {
        m_net.SendRoomListRes(ctx, rooms);
        ReleaseIO(ctx);
    }
}

void LobbyService::BroadcastRoomMemberList(uint32_t roomId)
{
    std::vector<RoomMemberInfoView> members;
    std::vector<ClientContext*> targets;

    AcquireSRWLockShared(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockShared(&m_lock);
        return;
    }

    const Room& room = itRoom->second;
    members = BuildRoomMemberListView_Unsafe(room);

    for (uint32_t sid : room.members)
    {
        auto itCtx = m_ctxBySession.find(sid);
        if (itCtx != m_ctxBySession.end())
        {
            ClientContext* ctx = itCtx->second;
            targets.push_back(ctx);
            AddIO(ctx);
        }
    }

    ReleaseSRWLockShared(&m_lock);

    LOBBY_TRACE("[ROOM] BROADCAST_MEMBER_LIST rid=%u targets=%zu members=%zu\n",
        roomId,
        targets.size(),
        members.size());

    for (ClientContext* ctx : targets)
    {
        m_net.SendRoomMemberList(ctx, roomId, members);
        ReleaseIO(ctx);
    }
}

bool LobbyService::ReadU32(const char* payload, uint16_t payloadLen, uint32_t& outHost)
{
    if (!payload || payloadLen < 4)
    {
        return false;
    }

    uint32_t net = 0;
    std::memcpy(&net, payload, 4);

    outHost = ntohl(net);
    return true;
}

static uint16_t ReadControlU16BE(const uint8_t* data)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) |
        static_cast<uint16_t>(data[1]));
}

static uint32_t ReadControlU32BE(const uint8_t* data)
{
    return
        (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

static uint64_t ReadControlU64BE(const uint8_t* data)
{
    uint64_t value = 0;
    for (int index = 0; index < 8; ++index)
    {
        value = (value << 8) | static_cast<uint64_t>(data[index]);
    }
    return value;
}

void LobbyService::HandleDediMatchEndNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlHeaderSize = 18;
    if (!payload || payloadLen < ControlHeaderSize + 1)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t notifyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    uint16_t offset = ControlHeaderSize;

    uint8_t winnerLen = p[offset++];
    if (offset + winnerLen > payloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad winner roomId=%u len=%u payloadLen=%u\n",
            roomId, winnerLen, payloadLen);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    std::string winner(payload + offset, payload + offset + winnerLen);
    offset += winnerLen;

    std::string summary;
    if (offset < payloadLen)
    {
        uint8_t summaryLen = p[offset++];

        if (offset + summaryLen > payloadLen)
        {
            LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad summary roomId=%u len=%u payloadLen=%u\\n",
                roomId, summaryLen, payloadLen);
            m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
            return;
        }

        summary.assign(payload + offset, payload + offset + summaryLen);
        offset += summaryLen;
    }

    bool shouldBroadcast = false;
    uint16_t oldPort = 0;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom != m_rooms.end())
    {
        Room& r = itRoom->second;

        if (r.state != RoomState::IN_GAME ||
            r.dedicatedPort != notifyPort ||
            r.dedicatedGeneration != generation ||
            r.dedicatedControlToken == 0 ||
            r.dedicatedControlToken != controlToken)
        {
            LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY rejected roomId=%u port=%u generation=%u state=%s expectedPort=%u expectedGeneration=%u\n",
                roomId,
                notifyPort,
                generation,
                RoomStateToString(r.state),
                r.dedicatedPort,
                r.dedicatedGeneration);
            ReleaseSRWLockExclusive(&m_lock);
            m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
            return;
        }

        oldPort = r.dedicatedPort;
        memberCount = r.members.size();

        if (r.dedicatedPort != 0 || r.dedicatedProcessHandle)
        {
            CleanupDedicatedServerForRoom_Unsafe(r, "MatchEnd", false);
        }

        r.state = RoomState::WAITING;

        for (auto itMember = r.members.begin(); itMember != r.members.end();)
        {
            const uint32_t memberSid = *itMember;
            if (m_ctxBySession.find(memberSid) == m_ctxBySession.end())
            {
                itMember = r.members.erase(itMember);
                r.readyStatus.erase(memberSid);
                r.handoverTickets.erase(memberSid);
                r.ghostDisconnectTimes.erase(memberSid);
                m_roomBySession.erase(memberSid);
                m_accountIdBySid.erase(memberSid);
                m_nicknameBySid.erase(memberSid);
                if (r.hostId == memberSid)
                {
                    r.hostId = 0;
                }
                continue;
            }

            ++itMember;
        }

        if (r.hostId != 0 &&
            std::find(r.members.begin(), r.members.end(), r.hostId) == r.members.end())
        {
            r.hostId = 0;
        }

        if (r.hostId == 0 && !r.members.empty())
        {
            r.hostId = r.members.front();
        }

        if (r.members.empty())
        {
            m_roomOrder.erase(
                std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                m_roomOrder.end());
            m_rooms.erase(itRoom);
            shouldBroadcast = true;
        }
        else
        {
            for (uint32_t sid : r.members)
            {
                r.readyStatus[sid] = (sid == r.hostId);
            }

            shouldBroadcast = true;
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, shouldBroadcast);

    LOBBY_INFO("[DEDI] MATCH_END_NOTIFY roomId=%u winner=%s summary=%s oldPort=%u members=%zu result=%s\n",
        roomId,
        winner.c_str(),
        summary.c_str(),
        oldPort,
        memberCount,
        shouldBroadcast ? "OK" : "ROOM_NOT_FOUND");

    if (shouldBroadcast)
    {
        BroadcastRoomList();
        BroadcastRoomMemberList(roomId);
    }
}

void LobbyService::HandleDediServerReadyNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlPayloadSize = 18;
    if (!payload || payloadLen < ControlPayloadSize)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t readyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);

    AcquireSRWLockExclusive(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY roomId=%u port=%u result=ROOM_NOT_FOUND\n",
            roomId,
            readyPort);
        m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, false);
        return;
    }

    Room& room = itRoom->second;
    const uint16_t roomPort = room.dedicatedPort;
    if (room.state != RoomState::IN_GAME ||
        readyPort == 0 ||
        roomPort != readyPort ||
        room.dedicatedGeneration != generation ||
        room.dedicatedControlToken == 0 ||
        room.dedicatedControlToken != controlToken)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY rejected roomId=%u readyPort=%u roomPort=%u generation=%u expectedGeneration=%u state=%s\n",
            roomId,
            readyPort,
            roomPort,
            generation,
            room.dedicatedGeneration,
            RoomStateToString(room.state));
        ReleaseSRWLockExclusive(&m_lock);
        m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, false);
        return;
    }

    room.dedicatedReadyReceived = true;
    LOBBY_INFO("[DEDI] SERVER_READY_NOTIFY roomId=%u port=%u generation=%u result=OK\n",
        roomId,
        roomPort,
        generation);

    m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, true);
    SendGameStartForRoom_Unsafe(roomId, "DediReady");

    ReleaseSRWLockExclusive(&m_lock);
}
