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

static const char* DEDI_PUBLIC_IP =
"127.0.0.1";

static constexpr size_t MIN_PLAYERS_TO_START = 1;

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

void LobbyService::CleanupDedicatedServerForRoom_Unsafe(Room& room, const char* reason, bool terminateProcess)
{
    const uint16_t oldPort = room.dedicatedPort;
    const HANDLE oldProcessHandle = room.dedicatedProcessHandle;
    const DWORD oldProcessId = room.dedicatedProcessId;

    room.dedicatedPort = 0;
    room.dedicatedProcessHandle = NULL;
    room.dedicatedProcessId = 0;
    room.gameStartSent = false;
    room.handoverTickets.clear();

    if (oldPort != 0)
    {
        FreePort(oldPort);
    }

    if (!oldProcessHandle)
    {
        if (oldPort != 0)
        {
            LOBBY_INFO("[DEDI] Cleanup roomId=%u reason=%s port=%u pid=0 result=PORT_ONLY\n",
                room.id,
                reason ? reason : "Unknown",
                oldPort);
        }
        return;
    }

    DWORD exitCode = STILL_ACTIVE;
    const bool hasExitCode = GetExitCodeProcess(oldProcessHandle, &exitCode) != FALSE;
    const bool stillActive = hasExitCode && exitCode == STILL_ACTIVE;

    if (stillActive && terminateProcess)
    {
        if (TerminateProcess(oldProcessHandle, 0))
        {
            LOBBY_INFO("[DEDI] Cleanup roomId=%u reason=%s port=%u pid=%lu result=TERMINATED\n",
                room.id,
                reason ? reason : "Unknown",
                oldPort,
                oldProcessId);
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
    else
    {
        LOBBY_INFO("[DEDI] Cleanup roomId=%u reason=%s port=%u pid=%lu result=%s exitCode=%lu\n",
            room.id,
            reason ? reason : "Unknown",
            oldPort,
            oldProcessId,
            stillActive ? "HANDLE_RELEASED" : "PROCESS_EXITED",
            hasExitCode ? exitCode : GetLastError());
    }

    CloseHandle(oldProcessHandle);
}

void LobbyService::SweepDedicatedServerProcesses()
{
    std::vector<uint32_t> changedRooms;

    AcquireSRWLockExclusive(&m_lock);

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

bool LobbyService::LaunchDedicatedServer(uint16_t port, uint32_t roomId, uint16_t requiredPlayers, const std::string& allowedTickets, HANDLE& outProcessHandle, DWORD& outProcessId)
{
    outProcessHandle = NULL;
    outProcessId = 0;

    char mapWithOptions[1024]{};
    sprintf_s(
        mapWithOptions,
        "%s?RoomId=%u?RequiredPlayers=%u?Tickets=%s",
        DEDI_MAP_PATH,
        roomId,
        static_cast<unsigned>(requiredPlayers),
        allowedTickets.c_str()
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
    SweepDedicatedServerProcesses();

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
    SweepDedicatedServerProcesses();

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
    SweepDedicatedServerProcesses();

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

                ReleaseSRWLockExclusive(&m_lock);

                printf("[AUTH] LOGIN id=%s sid=%u result=%s port=%u\n",
                    id.c_str(),
                    oldSid,
                    LoginResultToString(LoginResult::OK_RECONNECT),
                    port);

                m_net.SendLoginRes(c, LoginResult::OK_RECONNECT);
                m_net.SendGameStart(c, DEDI_PUBLIC_IP, port, reconnectTicket);
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

    ReleaseSRWLockExclusive(&m_lock);

    uint16_t requiredPlayers = static_cast<uint16_t>(membersCopy.size());
    HANDLE dediProcessHandle = NULL;
    DWORD dediProcessId = 0;

    if (!LaunchDedicatedServer(port, rid, requiredPlayers, allowedTickets, dediProcessHandle, dediProcessId))
    {
        AcquireSRWLockExclusive(&m_lock);

        auto itRollbackRoom = m_rooms.find(rid);
        if (itRollbackRoom != m_rooms.end())
        {
            itRollbackRoom->second.state = RoomState::WAITING;
            itRollbackRoom->second.dedicatedPort = 0;
            itRollbackRoom->second.dedicatedProcessHandle = NULL;
            itRollbackRoom->second.dedicatedProcessId = 0;
            itRollbackRoom->second.gameStartSent = false;
            itRollbackRoom->second.handoverTickets.clear();
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
        itStoreRoom->second.dedicatedPort == port)
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

    for (uint32_t mSid : membersCopy)
    {
        auto itCtx = m_ctxBySession.find(mSid);
        if (itCtx == m_ctxBySession.end())
        {
            LOBBY_WARN("[ROOM][WARN] GAME_START_TARGET_MISSING rid=%u sid=%u reason=%s\n",
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
            DEDI_PUBLIC_IP,
            port,
            reason ? reason : "Unknown");

        m_net.SendGameStart(itCtx->second, DEDI_PUBLIC_IP, port, itTicket->second);
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
void LobbyService::HandleDediMatchEndNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    if (!payload || payloadLen < 5)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);

    uint32_t roomId = 0;
    roomId |= static_cast<uint32_t>(p[0]) << 24;
    roomId |= static_cast<uint32_t>(p[1]) << 16;
    roomId |= static_cast<uint32_t>(p[2]) << 8;
    roomId |= static_cast<uint32_t>(p[3]);

    uint16_t offset = 4;

    uint8_t winnerLen = p[offset++];
    if (offset + winnerLen > payloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad winner roomId=%u len=%u payloadLen=%u\n",
            roomId, winnerLen, payloadLen);
        return;
    }

    std::string winner(payload + offset, payload + offset + winnerLen);
    offset += winnerLen;

    std::string summary;
    if (offset < payloadLen)
    {
        uint8_t summaryLen = p[offset++];

        if (offset + summaryLen <= payloadLen)
        {
            summary.assign(payload + offset, payload + offset + summaryLen);
            offset += summaryLen;
        }
    }

    bool shouldBroadcast = false;
    uint16_t oldPort = 0;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom != m_rooms.end())
    {
        Room& r = itRoom->second;

        oldPort = r.dedicatedPort;
        memberCount = r.members.size();

        if (r.dedicatedPort != 0 || r.dedicatedProcessHandle)
        {
            CleanupDedicatedServerForRoom_Unsafe(r, "MatchEnd", true);
        }

        r.state = RoomState::WAITING;

        for (uint32_t sid : r.members)
        {
            r.readyStatus[sid] = (sid == r.hostId);
        }

        shouldBroadcast = true;
    }

    ReleaseSRWLockExclusive(&m_lock);

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
    if (!payload || payloadLen < 4)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);

    uint32_t roomId = 0;
    roomId |= static_cast<uint32_t>(p[0]) << 24;
    roomId |= static_cast<uint32_t>(p[1]) << 16;
    roomId |= static_cast<uint32_t>(p[2]) << 8;
    roomId |= static_cast<uint32_t>(p[3]);

    uint16_t readyPort = 0;
    if (payloadLen >= 6)
    {
        readyPort |= static_cast<uint16_t>(p[4]) << 8;
        readyPort |= static_cast<uint16_t>(p[5]);
    }

    AcquireSRWLockExclusive(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY roomId=%u port=%u result=ROOM_NOT_FOUND\n",
            roomId,
            readyPort);
        return;
    }

    const uint16_t roomPort = itRoom->second.dedicatedPort;
    if (readyPort != 0 && roomPort != 0 && readyPort != roomPort)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY roomId=%u readyPort=%u roomPort=%u result=PORT_MISMATCH_CONTINUE\n",
            roomId,
            readyPort,
            roomPort);
    }
    else
    {
        LOBBY_INFO("[DEDI] SERVER_READY_NOTIFY roomId=%u port=%u result=OK\n",
            roomId,
            roomPort);
    }

    SendGameStartForRoom_Unsafe(roomId, "DediReady");

    ReleaseSRWLockExclusive(&m_lock);
}
