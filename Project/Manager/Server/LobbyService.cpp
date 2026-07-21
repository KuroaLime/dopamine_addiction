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
static constexpr int DEDI_GRACEFUL_SHUTDOWN_SECONDS = 15;
static constexpr int DEDI_TERMINATE_RETRY_SECONDS = 5;
static constexpr int DEDI_COMPLETED_CONTROL_TTL_SECONDS = 120;

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

static bool IsUtf8TextWithinCharacterLimit(const std::string& value, int maxCharacters)
{
    if (value.empty() || maxCharacters <= 0)
    {
        return false;
    }

#ifdef _WIN32
    const int utf16Length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    return utf16Length > 0 && utf16Length <= maxCharacters;
#else
    return value.size() <= static_cast<size_t>(maxCharacters);
#endif
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

static int GetEnvIntOrDefault(
    const char* name,
    int fallback,
    int minimum,
    int maximum)
{
    const std::string value = GetEnvStringOrDefault(name, "");
    if (value.empty())
    {
        return fallback;
    }

    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < minimum || parsed > maximum)
    {
        LOBBY_WARN("[CONFIG][WARN] Invalid %s=%s fallback=%d range=%d..%d\n",
            name,
            value.c_str(),
            fallback,
            minimum,
            maximum);
        return fallback;
    }

    return static_cast<int>(parsed);
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
extern void MarkClientClosing(struct ClientContext* c, const char* reason);


// ===========================================================================
// Constructor & Initialization
// ===========================================================================

LobbyService::LobbyService(NetApi& net)
    : m_net(net)
{
    InitializeSRWLock(&m_lock);
    m_unauthenticatedIdleTimeoutSeconds = GetEnvIntOrDefault(
        "MANAGER_IOCP_UNAUTH_IDLE_TIMEOUT_SECONDS",
        300,
        5,
        3600);
    InitPortPool(7777, 15);

    LOBBY_INFO("[ADMISSION] Unauthenticated idle timeout=%d seconds\n",
        m_unauthenticatedIdleTimeoutSeconds);

    m_dedicatedServerJob = CreateJobObjectW(nullptr, nullptr);
    if (!m_dedicatedServerJob)
    {
        LOBBY_ERR("[DEDI][ERR] CreateJobObject failed gle=%lu\n", GetLastError());
        return;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};
    jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(
        m_dedicatedServerJob,
        JobObjectExtendedLimitInformation,
        &jobInfo,
        sizeof(jobInfo)))
    {
        LOBBY_ERR("[DEDI][ERR] SetInformationJobObject failed gle=%lu\n", GetLastError());
        CloseHandle(m_dedicatedServerJob);
        m_dedicatedServerJob = NULL;
        return;
    }

    LOBBY_INFO("[DEDI] JobObject ready killOnClose=1\n");
}

LobbyService::~LobbyService()
{
    ShutdownDedicatedServers();
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
    const uint32_t oldGeneration = room.dedicatedGeneration;

    room.dedicatedPort = 0;
    room.dedicatedProcessHandle = NULL;
    room.dedicatedProcessId = 0;
    room.dedicatedControlToken = 0;
    room.dedicatedReadyReceived = false;
    room.dedicatedLaunchTime = {};
    room.gameStartSent = false;
    room.handoverTickets.clear();
    room.ghostDisconnectTimes.clear();

    if (oldProcessHandle)
    {
        RetiringDedicatedProcess retiring{};
        retiring.roomId = room.id;
        retiring.port = oldPort;
        retiring.processHandle = oldProcessHandle;
        retiring.processId = oldProcessId;
        retiring.generation = oldGeneration;
        retiring.reason = reason ? reason : "Unknown";
        retiring.terminateAfter = std::chrono::steady_clock::now() +
            std::chrono::seconds(terminateProcess ? 0 : DEDI_GRACEFUL_SHUTDOWN_SECONDS);
        m_retiringDedicatedProcesses.push_back(std::move(retiring));

        LOBBY_INFO("[DEDI] Retire queued roomId=%u reason=%s port=%u pid=%lu generation=%u terminateNow=%d retiring=%zu\n",
            room.id,
            reason ? reason : "Unknown",
            oldPort,
            oldProcessId,
            oldGeneration,
            terminateProcess ? 1 : 0,
            m_retiringDedicatedProcesses.size());
        return;
    }

    if (oldPort != 0)
    {
        if (IsUdpPortAvailable(oldPort))
        {
            FreePort(oldPort);
        }
        else
        {
            QuarantinePort_Unsafe(oldPort, "MissingProcessHandlePortStillBound");
        }
    }
}

bool LobbyService::RecoverRoomToWaiting_Unsafe(
    Room& room,
    const char* reason,
    bool terminateProcess)
{
    const size_t oldMemberCount = room.members.size();
    const uint16_t oldPort = room.dedicatedPort;
    const DWORD oldProcessId = room.dedicatedProcessId;

    CleanupDedicatedServerForRoom_Unsafe(room, reason, terminateProcess);
    room.state = RoomState::WAITING;

    size_t removedOfflineMembers = 0;
    for (auto itMember = room.members.begin(); itMember != room.members.end();)
    {
        const uint32_t memberSid = *itMember;
        const auto itContext = m_ctxBySession.find(memberSid);
        if (itContext == m_ctxBySession.end() || itContext->second == nullptr)
        {
            itMember = room.members.erase(itMember);
            room.readyStatus.erase(memberSid);
            room.handoverTickets.erase(memberSid);
            room.ghostDisconnectTimes.erase(memberSid);
            m_roomBySession.erase(memberSid);
            m_accountIdBySid.erase(memberSid);
            m_nicknameBySid.erase(memberSid);
            if (room.hostId == memberSid)
            {
                room.hostId = 0;
            }
            ++removedOfflineMembers;
            continue;
        }

        ++itMember;
    }

    if (room.hostId != 0 &&
        std::find(room.members.begin(), room.members.end(), room.hostId) == room.members.end())
    {
        room.hostId = 0;
    }
    if (room.hostId == 0 && !room.members.empty())
    {
        room.hostId = room.members.front();
    }

    room.readyStatus.clear();
    for (uint32_t memberSid : room.members)
    {
        room.readyStatus[memberSid] = (memberSid == room.hostId);
    }

    LOBBY_INFO("[RECOVERY] ROOM_TO_WAITING rid=%u reason=%s oldPort=%u oldPid=%lu members=%zu->%zu removedOffline=%zu host=%u terminate=%d empty=%d\n",
        room.id,
        reason ? reason : "Unknown",
        oldPort,
        oldProcessId,
        oldMemberCount,
        room.members.size(),
        removedOfflineMembers,
        room.hostId,
        terminateProcess ? 1 : 0,
        room.members.empty() ? 1 : 0);

    return room.members.empty();
}

bool LobbyService::WasDediControlCompleted_Unsafe(
    PacketType notifyType,
    uint32_t roomId,
    uint16_t port,
    uint32_t generation,
    uint64_t controlToken) const
{
    return std::any_of(
        m_completedDediControls.begin(),
        m_completedDediControls.end(),
        [notifyType, roomId, port, generation, controlToken](const CompletedDediControl& completed)
        {
            return completed.notifyType == notifyType &&
                completed.roomId == roomId &&
                completed.port == port &&
                completed.generation == generation &&
                completed.controlToken == controlToken;
        });
}

void LobbyService::RememberDediControlCompleted_Unsafe(
    PacketType notifyType,
    uint32_t roomId,
    uint16_t port,
    uint32_t generation,
    uint64_t controlToken)
{
    m_completedDediControls.erase(
        std::remove_if(
            m_completedDediControls.begin(),
            m_completedDediControls.end(),
            [notifyType, roomId, port, generation, controlToken](const CompletedDediControl& completed)
            {
                return completed.notifyType == notifyType &&
                    completed.roomId == roomId &&
                    completed.port == port &&
                    completed.generation == generation &&
                    completed.controlToken == controlToken;
            }),
        m_completedDediControls.end());

    CompletedDediControl completed{};
    completed.notifyType = notifyType;
    completed.roomId = roomId;
    completed.port = port;
    completed.generation = generation;
    completed.controlToken = controlToken;
    completed.completedAt = std::chrono::steady_clock::now();
    m_completedDediControls.push_back(std::move(completed));
}

void LobbyService::SweepCompletedDediControls_Unsafe(
    const std::chrono::steady_clock::time_point& now)
{
    m_completedDediControls.erase(
        std::remove_if(
            m_completedDediControls.begin(),
            m_completedDediControls.end(),
            [&now](const CompletedDediControl& completed)
            {
                return std::chrono::duration_cast<std::chrono::seconds>(
                    now - completed.completedAt).count() >=
                    DEDI_COMPLETED_CONTROL_TTL_SECONDS;
            }),
        m_completedDediControls.end());
}

void LobbyService::SweepRetiringDedicatedServers_Unsafe(
    const std::chrono::steady_clock::time_point& now)
{
    for (auto it = m_retiringDedicatedProcesses.begin();
        it != m_retiringDedicatedProcesses.end();)
    {
        RetiringDedicatedProcess& retiring = *it;
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(retiring.processHandle, &exitCode))
        {
            LOBBY_ERR("[DEDI][ERR] Retire handle invalid roomId=%u port=%u pid=%lu generation=%u reason=%s gle=%lu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.reason.c_str(),
                GetLastError());

            CloseHandle(retiring.processHandle);
            if (retiring.port != 0)
            {
                QuarantinePort_Unsafe(retiring.port, "RetireProcessHandleInvalid");
            }
            it = m_retiringDedicatedProcesses.erase(it);
            continue;
        }

        if (exitCode != STILL_ACTIVE)
        {
            CloseHandle(retiring.processHandle);

            const bool portAvailable = retiring.port == 0 || IsUdpPortAvailable(retiring.port);
            if (retiring.port != 0)
            {
                if (portAvailable)
                {
                    FreePort(retiring.port);
                }
                else
                {
                    QuarantinePort_Unsafe(retiring.port, "RetiredProcessExitedPortStillBound");
                }
            }

            LOBBY_INFO("[DEDI] Retire complete roomId=%u port=%u pid=%lu generation=%u exitCode=%lu portAvailable=%d reason=%s remaining=%zu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                exitCode,
                portAvailable ? 1 : 0,
                retiring.reason.c_str(),
                m_retiringDedicatedProcesses.size() - 1);

            it = m_retiringDedicatedProcesses.erase(it);
            continue;
        }

        if (now < retiring.terminateAfter)
        {
            ++it;
            continue;
        }

        if (TerminateProcess(retiring.processHandle, 0))
        {
            retiring.terminationRequested = true;
            retiring.terminateAfter = now + std::chrono::seconds(DEDI_TERMINATE_RETRY_SECONDS);
            LOBBY_WARN("[DEDI][WARN] Retire terminate requested roomId=%u port=%u pid=%lu generation=%u reason=%s\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.reason.c_str());
        }
        else
        {
            const DWORD terminateError = GetLastError();
            retiring.terminateAfter = now + std::chrono::seconds(DEDI_TERMINATE_RETRY_SECONDS);
            LOBBY_ERR("[DEDI][ERR] Retire terminate failed roomId=%u port=%u pid=%lu generation=%u requestedBefore=%d reason=%s gle=%lu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.terminationRequested ? 1 : 0,
                retiring.reason.c_str(),
                terminateError);
        }

        ++it;
    }
}

void LobbyService::SweepDedicatedServerProcesses()
{
    std::vector<uint32_t> changedRooms;
    bool shouldBroadcastRoomList = false;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);
    ReclaimQuarantinedPorts_Unsafe();
    SweepRetiringDedicatedServers_Unsafe(now);
    SweepCompletedDediControls_Unsafe(now);

    for (auto itRoom = m_rooms.begin(); itRoom != m_rooms.end();)
    {
        Room& room = itRoom->second;
        if (!room.dedicatedProcessHandle)
        {
            ++itRoom;
            continue;
        }

        const char* recoveryReason = nullptr;
        bool terminateProcess = false;
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(room.dedicatedProcessHandle, &exitCode))
        {
            LOBBY_ERR("[DEDI][ERR] Process handle invalid before recovery. roomId=%u port=%u pid=%lu gle=%lu\n",
                room.id,
                room.dedicatedPort,
                room.dedicatedProcessId,
                GetLastError());
            recoveryReason = "ProcessHandleInvalid";
        }
        else if (exitCode == STILL_ACTIVE)
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
                    recoveryReason = "ReadyTimeout";
                    terminateProcess = true;
                }
            }

            if (!recoveryReason)
            {
                ++itRoom;
                continue;
            }
        }
        else
        {
            LOBBY_ERR("[DEDI][ERR] Process exited before control cleanup. roomId=%u port=%u pid=%lu exitCode=%lu\n",
                room.id,
                room.dedicatedPort,
                room.dedicatedProcessId,
                exitCode);
            recoveryReason = "ProcessExited";
        }

        const uint32_t roomId = room.id;
        const bool roomEmpty = RecoverRoomToWaiting_Unsafe(
            room,
            recoveryReason,
            terminateProcess);
        shouldBroadcastRoomList = true;

        if (roomEmpty)
        {
            m_roomOrder.erase(
                std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                m_roomOrder.end());
            itRoom = m_rooms.erase(itRoom);
            LOBBY_INFO("[RECOVERY] EMPTY_ROOM_DELETED rid=%u reason=%s\n",
                roomId,
                recoveryReason);
            continue;
        }

        changedRooms.push_back(roomId);
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

void LobbyService::SweepUnauthenticatedClients()
{
    struct TimedOutClient
    {
        ClientContext* context = nullptr;
        uint32_t sessionId = 0;
        long long idleSeconds = 0;
    };

    std::vector<TimedOutClient> timedOutClients;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);

    for (auto& activityEntry : m_clientActivityByCtx)
    {
        ClientContext* context = activityEntry.first;
        auto itSession = m_sessionByCtx.find(context);
        if (!context || itSession == m_sessionByCtx.end())
        {
            continue;
        }

        const uint32_t sessionId = itSession->second;
        if (m_accountIdBySid.find(sessionId) != m_accountIdBySid.end())
        {
            continue;
        }

        const long long idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now - activityEntry.second.lastPacketAt).count();
        if (idleSeconds < m_unauthenticatedIdleTimeoutSeconds)
        {
            continue;
        }

        AddIO(context);
        timedOutClients.push_back({ context, sessionId, idleSeconds });
        activityEntry.second.lastPacketAt = now;
    }

    ReleaseSRWLockExclusive(&m_lock);

    for (const TimedOutClient& timedOut : timedOutClients)
    {
        LOBBY_WARN("[ADMISSION][WARN] Closing unauthenticated idle client sid=%u idle=%lld timeout=%d\n",
            timedOut.sessionId,
            timedOut.idleSeconds,
            m_unauthenticatedIdleTimeoutSeconds);
        MarkClientClosing(timedOut.context, "UnauthenticatedIdleTimeout");
        ReleaseIO(timedOut.context);
    }
}

void LobbyService::TickMaintenance()
{
    SweepDedicatedServerProcesses();
    SweepGhostSessions();
    SweepUnauthenticatedClients();
}

void LobbyService::ShutdownDedicatedServers()
{
    std::vector<HANDLE> processHandles;
    HANDLE jobHandle = NULL;

    AcquireSRWLockExclusive(&m_lock);
    if (m_dedicatedServerShutdown)
    {
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    m_dedicatedServerShutdown = true;

    for (auto& entry : m_rooms)
    {
        Room& room = entry.second;
        if (room.dedicatedProcessHandle)
        {
            processHandles.push_back(room.dedicatedProcessHandle);
            room.dedicatedProcessHandle = NULL;
        }
        room.dedicatedProcessId = 0;
        room.dedicatedPort = 0;
        room.dedicatedControlToken = 0;
        room.dedicatedReadyReceived = false;
        room.dedicatedLaunchTime = {};
        room.gameStartSent = false;
        room.handoverTickets.clear();
        room.ghostDisconnectTimes.clear();
    }

    for (RetiringDedicatedProcess& retiring : m_retiringDedicatedProcesses)
    {
        if (retiring.processHandle)
        {
            processHandles.push_back(retiring.processHandle);
            retiring.processHandle = NULL;
        }
    }
    m_retiringDedicatedProcesses.clear();

    jobHandle = m_dedicatedServerJob;
    m_dedicatedServerJob = NULL;
    ReleaseSRWLockExclusive(&m_lock);

    if (jobHandle)
    {
        if (!TerminateJobObject(jobHandle, 0))
        {
            LOBBY_ERR("[DEDI][ERR] TerminateJobObject during shutdown failed gle=%lu\n", GetLastError());
        }
    }

    size_t exitedCount = 0;
    for (HANDLE processHandle : processHandles)
    {
        if (!processHandle)
        {
            continue;
        }

        if (!jobHandle)
        {
            TerminateProcess(processHandle, 0);
        }

        const DWORD waitResult = WaitForSingleObject(processHandle, DEDI_TERMINATE_WAIT_MS);
        if (waitResult == WAIT_OBJECT_0)
        {
            ++exitedCount;
        }
        else
        {
            LOBBY_WARN("[DEDI][WARN] Shutdown wait incomplete wait=%lu\n", waitResult);
        }
        CloseHandle(processHandle);
    }

    if (jobHandle)
    {
        CloseHandle(jobHandle);
    }

    LOBBY_INFO("[DEDI] Shutdown complete tracked=%zu exited=%zu job=%d\n",
        processHandles.size(),
        exitedCount,
        jobHandle ? 1 : 0);
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

    if (!m_dedicatedServerJob)
    {
        LOBBY_ERR("[DEDI][ERR] Launch failed. roomId=%u port=%u result=JOB_OBJECT_NOT_READY\n",
            roomId,
            port);
        return false;
    }

    BOOL ok = CreateProcessA(
        nullptr,
        cmdLine,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
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

    if (!AssignProcessToJobObject(m_dedicatedServerJob, pi.hProcess))
    {
        const DWORD assignError = GetLastError();
        TerminateProcess(pi.hProcess, 0);
        WaitForSingleObject(pi.hProcess, DEDI_TERMINATE_WAIT_MS);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        LOBBY_ERR("[DEDI][ERR] Launch failed. roomId=%u port=%u pid=%lu result=JOB_ASSIGN_FAILED gle=%lu\n",
            roomId,
            port,
            pi.dwProcessId,
            assignError);
        return false;
    }

    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1))
    {
        const DWORD resumeError = GetLastError();
        TerminateProcess(pi.hProcess, 0);
        WaitForSingleObject(pi.hProcess, DEDI_TERMINATE_WAIT_MS);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        LOBBY_ERR("[DEDI][ERR] Launch failed. roomId=%u port=%u pid=%lu result=RESUME_FAILED gle=%lu\n",
            roomId,
            port,
            pi.dwProcessId,
            resumeError);
        return false;
    }

    LOBBY_INFO("[DEDI] Launch success. roomId=%u requiredPlayers=%u port=%u pid=%lu usePackaged=%d jobAssigned=1\n",
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
    m_clientActivityByCtx[c].lastPacketAt = std::chrono::steady_clock::now();

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

    m_clientActivityByCtx.erase(c);

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
        (pktType == PacketType::D2L_SERVER_READY_NOTIFY) ||
        (pktType == PacketType::D2L_MATCH_ABORT_NOTIFY);

    bool isLoggedIn = false;

    {
        AcquireSRWLockExclusive(&m_lock);

        auto itSession = m_sessionByCtx.find(c);
        if (itSession != m_sessionByCtx.end())
        {
            uint32_t sid = itSession->second;
            isLoggedIn = (m_accountIdBySid.find(sid) != m_accountIdBySid.end());

            auto itActivity = m_clientActivityByCtx.find(c);
            if (itActivity != m_clientActivityByCtx.end())
            {
                itActivity->second.lastPacketAt = std::chrono::steady_clock::now();
            }
        }

        ReleaseSRWLockExclusive(&m_lock);
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

    case PacketType::D2L_MATCH_ABORT_NOTIFY:
        HandleDediMatchAbortNotify(c, payload, payloadLen);
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
    if (!IsUtf8TextWithinCharacterLimit(outId, MAX_ID_CHAR_LEN))
    {
        return false;
    }

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

    const size_t authEnd = static_cast<size_t>(1 + idLen + 1 + pwLen);

    if (!outNickname)
    {
        return payloadLen == authEnd;
    }

    if (payloadLen == authEnd)
    {
        *outNickname = outId;
        return true;
    }

    if (payloadLen < authEnd + 1)
    {
        return false;
    }

    const uint8_t nickLen = static_cast<uint8_t>(payload[authEnd]);
    if (nickLen == 0 || nickLen > MAX_NICKNAME_LEN)
    {
        return false;
    }

    if (payloadLen != authEnd + 1 + nickLen)
    {
        return false;
    }

    outNickname->assign(payload + authEnd + 1, nickLen);
    if (!IsUtf8TextWithinCharacterLimit(*outNickname, MAX_NICKNAME_CHAR_LEN))
    {
        return false;
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

    if (titleLen == 0 ||
        titleLen > ROOM_TITLE_MAX ||
        payloadLen != static_cast<uint16_t>(1 + titleLen))
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

    if (m_rooms.size() >= static_cast<std::size_t>(ROOM_LIST_MAX_ROOMS))
    {
        const std::size_t roomCount = m_rooms.size();
        ReleaseSRWLockExclusive(&m_lock);

        LOBBY_WARN("[ROOM][WARN] CREATE rejected. room capacity reached rooms=%zu max=%u sid=%u\n",
            roomCount,
            static_cast<unsigned>(ROOM_LIST_MAX_ROOMS),
            sid);

        m_net.SendRoomCreateRes(c, RoomResult::FULL, nullptr);
        return RoomResult::FULL;
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
    if (!payload || payloadLen != 1 || (payload[0] != 0 && payload[0] != 1))
    {
        return;
    }

    bool isReady = (payload[0] != 0);

    uint32_t sid = 0;
    uint32_t rid = 0;
    size_t memberCount = 0;
    std::vector<ClientContext*> targets;

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
    memberCount = r.members.size();
    targets.reserve(memberCount);

    for (uint32_t memberSid : r.members)
    {
        auto itCtx = m_ctxBySession.find(memberSid);
        if (itCtx == m_ctxBySession.end() || itCtx->second == nullptr)
        {
            continue;
        }

        ClientContext* target = itCtx->second;
        AddIO(target);
        targets.push_back(target);
    }

    printf("[ROOM] READY sid=%u rid=%u value=%d\n",
        sid,
        rid,
        isReady ? 1 : 0);

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] READY_BROADCAST rid=%u members=%zu targets=%zu fromSid=%u value=%d\n",
        rid,
        memberCount,
        targets.size(),
        sid,
        isReady ? 1 : 0);

    for (ClientContext* target : targets)
    {
        m_net.SendRoomReadyBrd(target, sid, isReady);
        ReleaseIO(target);
    }

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
            WaitForSingleObject(dediProcessHandle, DEDI_TERMINATE_WAIT_MS);
            CloseHandle(dediProcessHandle);
        }

        AcquireSRWLockExclusive(&m_lock);
        if (IsUdpPortAvailable(port))
        {
            FreePort(port);
        }
        else
        {
            QuarantinePort_Unsafe(port, "RoomReleasedDuringLaunch");
        }
        ReleaseSRWLockExclusive(&m_lock);

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

bool LobbyService::PrepareGameStartForRoom_Unsafe(
    uint32_t roomId,
    const char* reason,
    uint16_t& outPort,
    std::string& outHost,
    std::vector<GameStartTarget>& outTargets)
{
    outPort = 0;
    outHost.clear();
    outTargets.clear();

    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        LOBBY_WARN("[ROOM][WARN] GAME_START_SKIP rid=%u reason=%s result=ROOM_NOT_FOUND\n",
            roomId,
            reason ? reason : "Unknown");
        return false;
    }

    Room& room = itRoom->second;
    if (room.state != RoomState::IN_GAME || room.dedicatedPort == 0)
    {
        LOBBY_WARN("[ROOM][WARN] GAME_START_SKIP rid=%u reason=%s state=%s port=%u result=ROOM_NOT_IN_GAME\n",
            roomId,
            reason ? reason : "Unknown",
            RoomStateToString(room.state),
            room.dedicatedPort);
        return false;
    }

    if (room.gameStartSent)
    {
        LOBBY_TRACE("[ROOM] GAME_START_SKIP rid=%u reason=%s port=%u result=ALREADY_SENT\n",
            roomId,
            reason ? reason : "Unknown",
            room.dedicatedPort);
        return false;
    }

    room.gameStartSent = true;
    outPort = room.dedicatedPort;
    outHost = GetDediAdvertisedHost();
    outTargets.reserve(room.members.size());

    for (uint32_t mSid : room.members)
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

        AddIO(itCtx->second);
        GameStartTarget target{};
        target.context = itCtx->second;
        target.sessionId = mSid;
        target.ticket = itTicket->second;
        outTargets.push_back(target);
    }

    return true;
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
    views.reserve((std::min)(
        m_roomOrder.size(),
        static_cast<std::size_t>(ROOM_LIST_MAX_ROOMS)));

    for (uint32_t rid : m_roomOrder)
    {
        if (views.size() >= static_cast<std::size_t>(ROOM_LIST_MAX_ROOMS))
        {
            break;
        }

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
    if (!payload || payloadLen != 4)
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
    static constexpr uint8_t MaxWinnerLength = 96;
    static constexpr uint8_t MaxSummaryLength = 180;
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

    const uint8_t winnerLen = p[offset++];
    if (winnerLen > MaxWinnerLength || offset + winnerLen > payloadLen)
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
        const uint8_t summaryLen = p[offset++];
        if (summaryLen > MaxSummaryLength || offset + summaryLen != payloadLen)
        {
            LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad summary roomId=%u len=%u payloadLen=%u\n",
                roomId, summaryLen, payloadLen);
            m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
            return;
        }

        summary.assign(payload + offset, payload + offset + summaryLen);
        offset += summaryLen;
    }

    if (offset != payloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY trailing payload roomId=%u payloadLen=%u parsed=%u\n",
            roomId, payloadLen, offset);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    bool accepted = false;
    bool recovered = false;
    bool duplicate = false;
    bool roomDeleted = false;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    if (WasDediControlCompleted_Unsafe(
        PacketType::D2L_MATCH_END_NOTIFY,
        roomId,
        notifyPort,
        generation,
        controlToken))
    {
        accepted = true;
        duplicate = true;
    }
    else
    {
        auto itRoom = m_rooms.find(roomId);
        if (itRoom != m_rooms.end())
        {
            Room& room = itRoom->second;
            if (room.state == RoomState::IN_GAME &&
                room.dedicatedPort == notifyPort &&
                room.dedicatedGeneration == generation &&
                room.dedicatedControlToken != 0 &&
                room.dedicatedControlToken == controlToken)
            {
                memberCount = room.members.size();
                RememberDediControlCompleted_Unsafe(
                    PacketType::D2L_MATCH_END_NOTIFY,
                    roomId,
                    notifyPort,
                    generation,
                    controlToken);

                roomDeleted = RecoverRoomToWaiting_Unsafe(room, "MatchEnd", false);
                if (roomDeleted)
                {
                    m_roomOrder.erase(
                        std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                        m_roomOrder.end());
                    m_rooms.erase(itRoom);
                }

                accepted = true;
                recovered = true;
            }
            else
            {
                LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY rejected roomId=%u port=%u generation=%u state=%s expectedPort=%u expectedGeneration=%u\n",
                    roomId,
                    notifyPort,
                    generation,
                    RoomStateToString(room.state),
                    room.dedicatedPort,
                    room.dedicatedGeneration);
            }
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(
        c,
        PacketType::L2D_MATCH_END_ACK,
        roomId,
        notifyPort,
        generation,
        controlToken,
        accepted);

    LOBBY_INFO("[DEDI] MATCH_END_NOTIFY roomId=%u winner=%s summary=%s port=%u members=%zu result=%s\n",
        roomId,
        winner.c_str(),
        summary.c_str(),
        notifyPort,
        memberCount,
        duplicate ? "DUPLICATE_ACK" : (recovered ? "RECOVERED" : "REJECTED"));

    if (recovered)
    {
        BroadcastRoomList();
        if (!roomDeleted)
        {
            BroadcastRoomMemberList(roomId);
        }
    }
}

void LobbyService::HandleDediMatchAbortNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlHeaderSize = 18;
    static constexpr uint8_t MaxReasonLength = 180;
    if (!payload || payloadLen < ControlHeaderSize + 1)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t notifyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    const uint8_t reasonLen = p[ControlHeaderSize];
    const uint16_t expectedPayloadLen = static_cast<uint16_t>(
        ControlHeaderSize + 1 + reasonLen);

    if (reasonLen > MaxReasonLength || payloadLen != expectedPayloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY bad reason roomId=%u len=%u payloadLen=%u expected=%u\n",
            roomId,
            reasonLen,
            payloadLen,
            expectedPayloadLen);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_ABORT_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    std::string reason(
        payload + ControlHeaderSize + 1,
        payload + ControlHeaderSize + 1 + reasonLen);
    std::replace(reason.begin(), reason.end(), '\r', ' ');
    std::replace(reason.begin(), reason.end(), '\n', ' ');
    if (reason.empty())
    {
        reason = "Unspecified";
    }

    bool accepted = false;
    bool recovered = false;
    bool duplicate = false;
    bool roomDeleted = false;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    if (WasDediControlCompleted_Unsafe(
        PacketType::D2L_MATCH_ABORT_NOTIFY,
        roomId,
        notifyPort,
        generation,
        controlToken))
    {
        accepted = true;
        duplicate = true;
    }
    else
    {
        auto itRoom = m_rooms.find(roomId);
        if (itRoom != m_rooms.end())
        {
            Room& room = itRoom->second;
            if (room.state == RoomState::IN_GAME &&
                room.dedicatedPort == notifyPort &&
                room.dedicatedGeneration == generation &&
                room.dedicatedControlToken != 0 &&
                room.dedicatedControlToken == controlToken)
            {
                memberCount = room.members.size();
                RememberDediControlCompleted_Unsafe(
                    PacketType::D2L_MATCH_ABORT_NOTIFY,
                    roomId,
                    notifyPort,
                    generation,
                    controlToken);

                roomDeleted = RecoverRoomToWaiting_Unsafe(
                    room,
                    reason.c_str(),
                    false);
                if (roomDeleted)
                {
                    m_roomOrder.erase(
                        std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                        m_roomOrder.end());
                    m_rooms.erase(itRoom);
                }

                accepted = true;
                recovered = true;
            }
            else
            {
                LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY rejected roomId=%u port=%u generation=%u state=%s expectedPort=%u expectedGeneration=%u reason=%s\n",
                    roomId,
                    notifyPort,
                    generation,
                    RoomStateToString(room.state),
                    room.dedicatedPort,
                    room.dedicatedGeneration,
                    reason.c_str());
            }
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(
        c,
        PacketType::L2D_MATCH_ABORT_ACK,
        roomId,
        notifyPort,
        generation,
        controlToken,
        accepted);

    LOBBY_WARN("[RECOVERY] MATCH_ABORT_NOTIFY roomId=%u port=%u generation=%u members=%zu reason=%s result=%s roomDeleted=%d\n",
        roomId,
        notifyPort,
        generation,
        memberCount,
        reason.c_str(),
        duplicate ? "DUPLICATE_ACK" : (recovered ? "RECOVERED" : "REJECTED"),
        roomDeleted ? 1 : 0);

    if (recovered)
    {
        BroadcastRoomList();
        if (!roomDeleted)
        {
            BroadcastRoomMemberList(roomId);
        }
    }
}

void LobbyService::HandleDediServerReadyNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlPayloadSize = 18;
    if (!payload || payloadLen != ControlPayloadSize)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t readyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    uint16_t gameStartPort = 0;
    std::string gameStartHost;
    std::vector<GameStartTarget> gameStartTargets;
    bool gameStartPrepared = false;

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

    gameStartPrepared = PrepareGameStartForRoom_Unsafe(
        roomId,
        "DediReady",
        gameStartPort,
        gameStartHost,
        gameStartTargets);

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, true);

    if (gameStartPrepared)
    {
        for (const GameStartTarget& target : gameStartTargets)
        {
            LOBBY_INFO("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u reason=DediReady ticketIssued=1\n",
                roomId,
                target.sessionId,
                gameStartHost.c_str(),
                gameStartPort);

            m_net.SendGameStart(
                target.context,
                gameStartHost.c_str(),
                gameStartPort,
                target.ticket);
            ReleaseIO(target.context);
        }
    }
}
