// LobbyService.cpp

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LobbyService.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>


// ===========================================================================
// Dedicated Server Settings
// ===========================================================================

static const char* DEDI_EDITOR_ENV =
"MANAGER_UE_EDITOR_EXE";

static const char* DEDI_PACKAGED_SERVER_ENV =
"MANAGER_DEDI_SERVER_EXE";

static const char* DEDI_EXE_REL_PATH =
"..\\..\\..\\..\\..\\UE\\UE_5.7_Source\\Engine\\Binaries\\Win64\\UnrealEditor.exe";

static const char* DEDI_EXE_FALLBACK_PATH =
"S:\\UE\\UE_5.7_Source\\Engine\\Binaries\\Win64\\UnrealEditor.exe";

static const char* DEDI_PROJECT_REL_PATH =
"..\\..\\..\\Manager.uproject";

static const char* DEDI_WORKING_DIR_REL_PATH =
"..\\..\\..";

static const char* DEDI_MAP_PATH =
"/Game/InGame/System/Main_Game_World";

static const char* DEDI_PUBLIC_IP =
"127.0.0.1";

static constexpr size_t MIN_PLAYERS_TO_START = 1;
static constexpr int DEDI_GAME_START_DELAY_SECONDS = 15;

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

    std::string relativePath = MakeAbsoluteFromExeDir(DEDI_EXE_REL_PATH);
    if (FileExists(relativePath))
    {
        return relativePath;
    }

    return DEDI_EXE_FALLBACK_PATH;
}

static std::string GetDediProjectPath()
{
    return MakeAbsoluteFromExeDir(DEDI_PROJECT_REL_PATH);
}

static std::string GetDediWorkingDir()
{
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
    if (port != 0)
    {
        m_freePorts.push_back(port);
    }
}


// ===========================================================================
// Dedicated Server Launch
// ===========================================================================

bool LobbyService::LaunchDedicatedServer(uint16_t port, uint32_t roomId, uint16_t requiredPlayers)
{
    char mapWithOptions[512]{};
    sprintf_s(
        mapWithOptions,
        "%s?RoomId=%u?RequiredPlayers=%u",
        DEDI_MAP_PATH,
        roomId,
        static_cast<unsigned>(requiredPlayers)
    );

    const std::string packagedServerExePath = GetPackagedServerExePath();
    const bool bUsePackagedServer = !packagedServerExePath.empty();

    std::string workingDir;
    char cmdLine[2048]{};

    if (bUsePackagedServer)
    {
        workingDir = GetParentDirectory(packagedServerExePath);

        printf("[DEDI] ResolvePackagedServer serverExe=%s workingDir=%s serverExists=%d workingDirExists=%d\n",
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

        printf("[DEDI] ResolveEditorServer editor=%s project=%s workingDir=%s editorExists=%d projectExists=%d workingDirExists=%d\n",
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

        printf("[DEDI] Launch failed. roomId=%u requiredPlayers=%u port=%u err=%lu cmd=%s\n",
            roomId,
            static_cast<unsigned>(requiredPlayers),
            port,
            err,
            cmdLine);

        return false;
    }

    printf("[DEDI] Launch success. roomId=%u requiredPlayers=%u port=%u pid=%lu cmd=%s\n",
        roomId,
        static_cast<unsigned>(requiredPlayers),
        port,
        pi.dwProcessId,
        cmdLine);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

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

    printf("[LOBBY] ACCEPT sid=%u\n", sid);

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

        printf("[LOBBY] DISCONNECT sid=%u rid=%u\n", sid, oldRid);

        if (oldRid != 0 && m_rooms.count(oldRid) > 0)
        {
            Room& r = m_rooms[oldRid];

            if (r.state != RoomState::IN_GAME)
            {
                printf("[LOBBY] DISCONNECT sid=%u leave room internally\n", sid);
                LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
            }
            else
            {
                printf("[LOBBY] DISCONNECT sid=%u stays as ghost (IN_GAME)\n", sid);
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
        (pktType == PacketType::C2S_PING);

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

            if (r.state == RoomState::IN_GAME)
            {
                uint16_t port = r.dedicatedPort;

                ReleaseSRWLockExclusive(&m_lock);

                printf("[AUTH] LOGIN id=%s sid=%u result=%s port=%u\n",
                    id.c_str(),
                    oldSid,
                    LoginResultToString(LoginResult::OK_RECONNECT),
                    port);

                m_net.SendLoginRes(c, LoginResult::OK_RECONNECT);
                m_net.SendGameStart(c, DEDI_PUBLIC_IP, port, oldSid);
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

    // Important:
    // Reserve the room as IN_GAME before launching Dedi.
    // This prevents duplicate Start requests from launching 7777, 7778, ...
    r.state = RoomState::IN_GAME;
    r.dedicatedPort = port;

    ReleaseSRWLockExclusive(&m_lock);

    uint16_t requiredPlayers = static_cast<uint16_t>(membersCopy.size());

    if (!LaunchDedicatedServer(port, rid, requiredPlayers))
    {
        AcquireSRWLockExclusive(&m_lock);

        auto itRollbackRoom = m_rooms.find(rid);
        if (itRollbackRoom != m_rooms.end())
        {
            itRollbackRoom->second.state = RoomState::WAITING;
            itRollbackRoom->second.dedicatedPort = 0;
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

    printf("[ROOM] START sid=%u rid=%u result=%s port=%u members=%zu\n",
        sid,
        rid,
        RoomResultToString(RoomResult::OK),
        port,
        membersCopy.size());

    m_net.SendRoomStartRes(c, RoomResult::OK);

    printf("[ROOM] GAME_START_DELAY rid=%u port=%u seconds=%d\n",
        rid,
        port,
        DEDI_GAME_START_DELAY_SECONDS);

    std::thread([this, rid, port, membersCopy]()
        {
            std::this_thread::sleep_for(std::chrono::seconds(DEDI_GAME_START_DELAY_SECONDS));

            AcquireSRWLockShared(&m_lock);

            for (uint32_t mSid : membersCopy)
            {
                auto itCtx = m_ctxBySession.find(mSid);

                if (itCtx != m_ctxBySession.end())
                {
                    printf("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u\n",
                        rid,
                        mSid,
                        DEDI_PUBLIC_IP,
                        port);

                    m_net.SendGameStart(itCtx->second, DEDI_PUBLIC_IP, port, mSid);
                }
            }

            ReleaseSRWLockShared(&m_lock);
        }).detach();

    BroadcastRoomList();
    BroadcastRoomMemberList(rid);
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

    if (r.hostId == sid)
    {
        if (!r.members.empty())
        {
            uint32_t oldHost = sid;
            r.hostId = r.members.front();
            r.readyStatus[r.hostId] = true;

            printf("[ROOM] HOST_CHANGE rid=%u oldHost=%u newHost=%u\n",
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

        if (r.dedicatedPort != 0)
        {
            FreePort(r.dedicatedPort);
            r.dedicatedPort = 0;
        }

        m_roomOrder.erase(
            std::remove(m_roomOrder.begin(), m_roomOrder.end(), rid),
            m_roomOrder.end());

        m_rooms.erase(itRoom);

        printf("[ROOM] EMPTY_DELETE rid=%u freePort=%u\n",
            rid,
            oldPort);

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

        printf("[ROOM] BROADCAST_TARGET sid=%u\n", sid);

        targets.push_back(ctx);
        AddIO(ctx);
    }

    ReleaseSRWLockShared(&m_lock);

    printf("[ROOM] BROADCAST_ROOM_LIST targets=%zu rooms=%zu\n",
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

    printf("[ROOM] BROADCAST_MEMBER_LIST rid=%u targets=%zu members=%zu\n",
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