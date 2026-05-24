// LobbyService.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LobbyService.h"
#include <cstring>
#include <algorithm> 
<<<<<<< Updated upstream
#include <cstdio>
#include <direct.h>
=======
#include <cstdio>    
#include <string>
#include <vector>
#include <filesystem>
>>>>>>> Stashed changes

#ifdef _WIN32
#include <winsock2.h>
#endif

// ===========================================================================
// Dedicated Server Settings
// ===========================================================================
// 기준: IOCP 서버 실행 파일 또는 작업 디렉토리가 Manager/Server 쪽에 있다고 가정.
// 절대 경로 X. 프로젝트 폴더 구조 기준 상대 경로 사용.

<<<<<<< Updated upstream
// 실행할 Unreal 맵 경로
// 실제 맵 경로가 다르면 여기만 바꾸면 됨.
// 현재는 ManagerServer.exe가 Cook/AssetRegistry 문제로 터지므로,
// 테스트 단계에서는 UnrealEditor-Cmd.exe를 서버처럼 실행한다.
// 나중에 Dedicated Server Cook/Stage가 끝나면 DEDI_EXE_PATH를
// 패키징된 ManagerServer.exe 경로로 다시 바꾸면 된다.

static const char* DEDI_EXE_PATH =
"S:\\UE\\UE_5.7_Source\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe";

static const char* DEDI_PROJECT_PATH =
"X:\\Project\\Manager\\Manager.uproject";

static const char* DEDI_WORKING_DIR =
"X:\\Project\\Manager\\";

static const char* DEDI_MAP_PATH =
"/Game/ThirdPerson/Lvl_ThirdPerson";

// 같은 PC 테스트는 127.0.0.1.
// 다른 PC 클라이언트 접속이면 서버 PC의 실제 LAN IP로 변경.
=======
static const char* DEDI_EXE_REL_PATH =
"..\\Binaries\\Win64\\ManagerServer.exe";

static const char* DEDI_PROJECT_REL_PATH =
"..\\Manager.uproject";

static const char* DEDI_WORKING_REL_DIR =
"..\\";

// 테스트 중 확인된 맵.
// Lobby_Stage를 쓸 거면 여기만 "/Game/Lobby/Lobby_Stage"로 바꾸면 됨.
static const char* DEDI_MAP_PATH =
"/Game/ThirdPerson/Lvl_ThirdPerson";

>>>>>>> Stashed changes
static const char* DEDI_PUBLIC_IP =
"127.0.0.1";
// 디버깅용 출력
const char* LoginResultToString(LoginResult r) {
    switch (r) {
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

const char* RoomResultToString(RoomResult r) {
    switch (r) {
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

const char* RoomStateToString(RoomState s) {
    switch (s) {
    case RoomState::WAITING: return "WAITING";
    case RoomState::IN_GAME: return "IN_GAME";
    default:                 return "UNKNOWN_ROOM_STATE";
    }
}

extern void AddIO(struct ClientContext* c);
extern void ReleaseIO(struct ClientContext* c);

static std::string GetProcessDir()
{
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    std::filesystem::path p(path);
    return p.parent_path().string();
}

static std::string MakeAbsFromProcessDir(const char* relPath)
{
    std::filesystem::path base = GetProcessDir();
    std::filesystem::path full = base / relPath;

    return std::filesystem::weakly_canonical(full).string();
}

// ===========================================================================
// Constructor & Initialization
// ===========================================================================
LobbyService::LobbyService(NetApi& net) : m_net(net) {
    InitializeSRWLock(&m_lock);
    InitPortPool(7777, 15);

    AcquireSRWLockExclusive(&m_lock);
    SeedRoomsForTest_Unsafe();
    ReleaseSRWLockExclusive(&m_lock);
}

// ===========================================================================
// Port Management (Resource Pool)
// ===========================================================================
void LobbyService::InitPortPool(uint16_t start, int count) {
    m_freePorts.reserve(count);
    for (int i = count - 1; i >= 0; --i) m_freePorts.push_back(start + i);
}

uint16_t LobbyService::AllocPort() {
    if (m_freePorts.empty()) return 0;
    uint16_t p = m_freePorts.back();
    m_freePorts.pop_back();
    return p;
}

void LobbyService::FreePort(uint16_t port) {
    if (port != 0) m_freePorts.push_back(port);
}

bool LobbyService::LaunchDedicatedServer(uint16_t port)
{
<<<<<<< Updated upstream
    DWORD exeAttr = GetFileAttributesA(DEDI_EXE_PATH);
    if (exeAttr == INVALID_FILE_ATTRIBUTES)
    {
        printf("[DEDI] EXE not found: %s\n", DEDI_EXE_PATH);
        return false;
    }

    DWORD projectAttr = GetFileAttributesA(DEDI_PROJECT_PATH);
    if (projectAttr == INVALID_FILE_ATTRIBUTES)
    {
        printf("[DEDI] Project file not found: %s\n", DEDI_PROJECT_PATH);
        return false;
    }

    DWORD workAttr = GetFileAttributesA(DEDI_WORKING_DIR);
    if (workAttr == INVALID_FILE_ATTRIBUTES || !(workAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        printf("[DEDI] Working directory not found: %s\n", DEDI_WORKING_DIR);
        return false;
    }

    char cmdLine[4096]{};

    sprintf_s(
        cmdLine,
        "\"%s\" \"%s\" %s -server -log -stdout -FullStdOutLogOutput -port=%u",
        DEDI_EXE_PATH,
        DEDI_PROJECT_PATH,
        DEDI_MAP_PATH,
        port
    );
=======
    std::string exePath = MakeAbsFromProcessDir(DEDI_EXE_REL_PATH);
    std::string projectPath = MakeAbsFromProcessDir(DEDI_PROJECT_REL_PATH);
    std::string workDir = MakeAbsFromProcessDir(DEDI_WORKING_REL_DIR);

    DWORD exeAttr = GetFileAttributesA(exePath.c_str());
    if (exeAttr == INVALID_FILE_ATTRIBUTES)
    {
        printf("[DEDI] EXE not found: %s\n", exePath.c_str());
        return false;
    }

    DWORD projectAttr = GetFileAttributesA(projectPath.c_str());
    if (projectAttr == INVALID_FILE_ATTRIBUTES)
    {
        printf("[DEDI] Project file not found: %s\n", projectPath.c_str());
        return false;
    }

    DWORD workAttr = GetFileAttributesA(workDir.c_str());
    if (workAttr == INVALID_FILE_ATTRIBUTES || !(workAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        printf("[DEDI] Working directory not found: %s\n", workDir.c_str());
        return false;
    }

    std::filesystem::path logPath =
        std::filesystem::path(workDir) / "Saved" / "Logs" /
        ("Dedi_" + std::to_string(port) + ".log");

    std::string cmdLine =
        "\"" + exePath + "\" "
        "\"" + projectPath + "\" "
        + std::string(DEDI_MAP_PATH) +
        " -server"
        " -log"
        " -forcelogflush"
        " -NullRHI"
        " -NoSound"
        " -port=" + std::to_string(port) +
        " -abslog=\"" + logPath.string() + "\"";
>>>>>>> Stashed changes

    STARTUPINFOA si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};

    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    BOOL ok = CreateProcessA(
<<<<<<< Updated upstream
        DEDI_EXE_PATH,
        cmdLine,
=======
        exePath.c_str(),
        mutableCmd.data(),
>>>>>>> Stashed changes
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
<<<<<<< Updated upstream
        DEDI_WORKING_DIR,
=======
        workDir.c_str(),
>>>>>>> Stashed changes
        &si,
        &pi
    );

    if (!ok)
    {
        DWORD err = GetLastError();

        printf("[DEDI] Launch failed. port=%u err=%lu\n", port, err);
<<<<<<< Updated upstream
        printf("[DEDI] exe=%s\n", DEDI_EXE_PATH);
        printf("[DEDI] project=%s\n", DEDI_PROJECT_PATH);
        printf("[DEDI] workdir=%s\n", DEDI_WORKING_DIR);
        printf("[DEDI] cmd=%s\n", cmdLine);
=======
        printf("[DEDI] exe=%s\n", exePath.c_str());
        printf("[DEDI] project=%s\n", projectPath.c_str());
        printf("[DEDI] workdir=%s\n", workDir.c_str());
        printf("[DEDI] cmd=%s\n", cmdLine.c_str());
>>>>>>> Stashed changes

        return false;
    }

<<<<<<< Updated upstream
    printf("[DEDI] Launch success. port=%u pid=%lu\n", port, pi.dwProcessId);
    printf("[DEDI] exe=%s\n", DEDI_EXE_PATH);
    printf("[DEDI] project=%s\n", DEDI_PROJECT_PATH);
    printf("[DEDI] workdir=%s\n", DEDI_WORKING_DIR);
    printf("[DEDI] cmd=%s\n", cmdLine);
=======
    Sleep(3000);

    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        if (exitCode != STILL_ACTIVE)
        {
            printf("[DEDI] Process exited too early. port=%u exitCode=%lu\n",
                port, exitCode);

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            return false;
        }
    }

    printf("[DEDI] Launch success. port=%u pid=%lu\n", port, pi.dwProcessId);
    printf("[DEDI] exe=%s\n", exePath.c_str());
    printf("[DEDI] project=%s\n", projectPath.c_str());
    printf("[DEDI] workdir=%s\n", workDir.c_str());
    printf("[DEDI] cmd=%s\n", cmdLine.c_str());
    printf("[DEDI] log=%s\n", logPath.string().c_str());
>>>>>>> Stashed changes

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
}

// ===========================================================================
// Connection Event Handlers
// ===========================================================================
void LobbyService::OnClientAccepted(ClientContext* c) {
    AcquireSRWLockExclusive(&m_lock);
    uint32_t sid = m_nextSessionId++;
    m_sessionByCtx[c] = sid;
    m_ctxBySession[sid] = c;
    m_roomBySession[sid] = 0;

    printf("[LOBBY] ACCEPT sid=%u\n", sid);
    
    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendWelcome(c, sid);
}

void LobbyService::OnClientDisconnected(ClientContext* c) {
    bool shouldBroadcast = false;

    AcquireSRWLockExclusive(&m_lock);

    auto it = m_sessionByCtx.find(c);
    if (it != m_sessionByCtx.end()) {
        uint32_t sid = it->second;
        uint32_t rid = 0;

        auto itRoomBySession = m_roomBySession.find(sid);
        if (itRoomBySession != m_roomBySession.end()) {
            rid = itRoomBySession->second;
        }

        printf("[LOBBY] DISCONNECT sid=%u rid=%u\n", sid, rid);

        // 방에 있고, 게임 중이 아니라면 내부적으로만 퇴장 처리
        if (rid != 0 && m_rooms.count(rid) > 0) {
            Room& r = m_rooms[rid];
            if (r.state != RoomState::IN_GAME) {
                printf("[LOBBY] DISCONNECT sid=%u leave room internally\n", sid);
                LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
            }
            else {
                printf("[LOBBY] DISCONNECT sid=%u stays as ghost (IN_GAME)\n", sid);
            }
        }

        // 통신 컨텍스트 매핑 제거
        m_ctxBySession.erase(sid);
        m_sessionByCtx.erase(it);

        // m_accountIdBySid는 남겨둠 (재접속 대비)
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (shouldBroadcast) {
        BroadcastRoomList();
    }
}

// ===========================================================================
// Packet Dispatcher
// ===========================================================================
void LobbyService::OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen) {
    PacketType pktType = static_cast<PacketType>(type);

    // 1) 로그인 없이 허용할 패킷인지 먼저 판정
    bool allowedWithoutLogin =
        (pktType == PacketType::C2S_LOGIN_REQ) ||
        (pktType == PacketType::C2S_REGISTER_REQ) ||
        (pktType == PacketType::C2S_PING);

    // 2) 로그인 여부 검사
    bool isLoggedIn = false;
    {
        AcquireSRWLockShared(&m_lock);

        auto itSession = m_sessionByCtx.find(c);
        if (itSession != m_sessionByCtx.end()) {
            uint32_t sid = itSession->second;
            isLoggedIn = (m_accountIdBySid.find(sid) != m_accountIdBySid.end());
        }

        ReleaseSRWLockShared(&m_lock);
    }

    // 3) 로그인 전인데 허용되지 않은 패킷이면 무시
    if (!isLoggedIn && !allowedWithoutLogin) {
        return;
    }

    // 4) 디스패치
    switch (pktType) {
    case PacketType::C2S_LOGIN_REQ:       HandleLoginReq(c, payload, payloadLen); break;
    case PacketType::C2S_REGISTER_REQ:    HandleRegisterReq(c, payload, payloadLen); break;

    case PacketType::C2S_ROOM_LIST_REQ:   HandleRoomListReq(c); break;
    case PacketType::C2S_ROOM_CREATE_REQ: HandleRoomCreateReq(c, payload, payloadLen); break;
    case PacketType::C2S_ROOM_JOIN_REQ:   HandleRoomJoinReq(c, payload, payloadLen); break;
    case PacketType::C2S_ROOM_LEAVE_REQ:  HandleRoomLeaveReq(c); break;

    case PacketType::C2S_ROOM_READY_REQ:  HandleRoomReadyReq(c, payload, payloadLen); break;
    case PacketType::C2S_ROOM_START_REQ:  HandleRoomStartReq(c); break;

    case PacketType::C2S_PING:            m_net.SendPong(c); break;
    default: break;
    }
}

// ===========================================================================
// Auth Logic
// ===========================================================================

// ID, PW 파싱 도구
bool LobbyService::ParseAuthPayload(
    const char* payload,
    uint16_t payloadLen,
    std::string& outId,
    std::string& outPw,
    std::string* outNickname
) {
    if (payloadLen < 1) return false;

    uint8_t idLen = static_cast<uint8_t>(payload[0]);
    if (idLen == 0 || idLen > MAX_ID_LEN) return false;
    if (payloadLen < 1 + idLen + 1) return false;

    outId.assign(payload + 1, idLen);

    uint8_t pwLen = static_cast<uint8_t>(payload[1 + idLen]);
    if (pwLen == 0 || pwLen > MAX_PW_LEN) return false;
    if (payloadLen < 1 + idLen + 1 + pwLen) return false;

    outPw.assign(payload + 1 + idLen + 1, pwLen);

    // Register용 optional nickname
    // 예전 클라처럼 id/pw만 보내면 nickname은 id로 처리.
    if (outNickname) {
        size_t nickOffset = 1 + idLen + 1 + pwLen;

        if (payloadLen > nickOffset) {
            if (payloadLen < nickOffset + 1) return false;

            uint8_t nickLen = static_cast<uint8_t>(payload[nickOffset]);

            if (nickLen == 0 || nickLen > MAX_NICKNAME_LEN) {
                return false;
            }

            if (payloadLen < nickOffset + 1 + nickLen) {
                return false;
            }

            outNickname->assign(payload + nickOffset + 1, nickLen);
        }
        else {
            *outNickname = outId;
        }
    }

    return true;
}

void LobbyService::HandleRegisterReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    std::string id, pw, nickname;

    if (!ParseAuthPayload(payload, payloadLen, id, pw, &nickname)) {
        printf("[AUTH] REGISTER result=%s\n", LoginResultToString(LoginResult::INVALID_FORMAT));
        m_net.SendRegisterRes(c, LoginResult::INVALID_FORMAT);
        return;
    }

    if (nickname.empty()) {
        nickname = id;
    }

    AcquireSRWLockExclusive(&m_lock);

    if (m_userDB.find(id) != m_userDB.end()) {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[AUTH] REGISTER id=%s result=%s\n",
            id.c_str(), LoginResultToString(LoginResult::ID_ALREADY_EXISTS));

        m_net.SendRegisterRes(c, LoginResult::ID_ALREADY_EXISTS);
        return;
    }

    UserRecord record;
    record.password = pw;
    record.nickname = nickname;

    m_userDB[id] = record;

    ReleaseSRWLockExclusive(&m_lock);

    printf("[AUTH] REGISTER id=%s nickname=%s result=%s\n",
        id.c_str(), nickname.c_str(), LoginResultToString(LoginResult::OK));

    m_net.SendRegisterRes(c, LoginResult::OK);
}

void LobbyService::HandleLoginReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    std::string id, pw;

    if (!ParseAuthPayload(payload, payloadLen, id, pw)) {
        printf("[AUTH] LOGIN result=%s\n", LoginResultToString(LoginResult::INVALID_FORMAT));
        m_net.SendLoginRes(c, LoginResult::INVALID_FORMAT);
        return;
    }

    AcquireSRWLockExclusive(&m_lock);
    auto it = m_userDB.find(id);
    if (it == m_userDB.end()) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[AUTH] LOGIN id=%s result=%s\n",
            id.c_str(), LoginResultToString(LoginResult::ID_NOT_FOUND));
        m_net.SendLoginRes(c, LoginResult::ID_NOT_FOUND);
        return;
    }
    if (it->second.password != pw) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[AUTH] LOGIN id=%s result=%s\n",
            id.c_str(), LoginResultToString(LoginResult::WRONG_PASSWORD));
        m_net.SendLoginRes(c, LoginResult::WRONG_PASSWORD);
        return;
    }

    // [재접속 검사] 이 아이디가 예전에 접속한 적이 있는가?
    uint32_t oldSid = 0;
    for (const auto& kv : m_accountIdBySid) {
        if (kv.second == id) { oldSid = kv.first; break; }
    }

    if (oldSid != 0) {
        if (m_ctxBySession.count(oldSid) > 0) {
            ReleaseSRWLockExclusive(&m_lock);
            printf("[AUTH] LOGIN id=%s result=%s\n",
                id.c_str(), LoginResultToString(LoginResult::ALREADY_LOGGED_IN));
            m_net.SendLoginRes(c, LoginResult::ALREADY_LOGGED_IN);
            return;
        }

        // 유령 상태의 옛날 세션을 찾음 -> 재접속 처리
        uint32_t currentSid = m_sessionByCtx[c];

        // 새로 부여받았던 임시 세션을 지우고, 옛날 세션 번호를 이 클라이언트에 물려줌
        m_ctxBySession.erase(currentSid);
        m_roomBySession.erase(currentSid);
        m_accountIdBySid.erase(currentSid);
        m_nicknameBySid.erase(currentSid);

        m_sessionByCtx[c] = oldSid;
        m_ctxBySession[oldSid] = c;
        m_nicknameBySid[oldSid] = it->second.nickname;

        uint32_t rid = m_roomBySession[oldSid];
        if (rid != 0 && m_rooms.count(rid) > 0) {
            Room& r = m_rooms[rid];
            if (r.state == RoomState::IN_GAME) {
                ReleaseSRWLockExclusive(&m_lock);

                printf("[AUTH] LOGIN id=%s sid=%u result=%s port=%u\n",
                    id.c_str(), oldSid, LoginResultToString(LoginResult::OK_RECONNECT), r.dedicatedPort);

                m_net.SendLoginRes(c, LoginResult::OK_RECONNECT);
                m_net.SendGameStart(c, DEDI_PUBLIC_IP, r.dedicatedPort, 0);
                return;
            }
        }
    }
    else {
        // 완전 첫 로그인
        uint32_t currentSid = m_sessionByCtx[c];
        m_accountIdBySid[currentSid] = id;
        m_nicknameBySid[currentSid] = it->second.nickname;
    }

    uint32_t loginSid = m_sessionByCtx[c];
    ReleaseSRWLockExclusive(&m_lock);

    printf("[AUTH] LOGIN id=%s sid=%u result=%s\n",
        id.c_str(), loginSid, LoginResultToString(LoginResult::OK));

    m_net.SendLoginRes(c, LoginResult::OK);
    HandleRoomListReq(c);
}

// ===========================================================================
// Room Logic Handlers
// ===========================================================================

RoomResult LobbyService::HandleRoomListReq(ClientContext* c) {
    AcquireSRWLockShared(&m_lock);
    auto views = BuildRoomListView_Unsafe();
    ReleaseSRWLockShared(&m_lock);
    m_net.SendRoomListRes(c, views);
    return RoomResult::OK;
}

RoomResult LobbyService::HandleRoomCreateReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    if (payloadLen < 1) {
        printf("[ROOM] CREATE result=%s\n", RoomResultToString(RoomResult::BAD_PAYLOAD));
        return RoomResult::BAD_PAYLOAD;
    }
    uint8_t titleLen = (uint8_t)payload[0];
    if (titleLen > ROOM_TITLE_MAX || payloadLen < (1 + titleLen)) {
        printf("[ROOM] CREATE result=%s\n", RoomResultToString(RoomResult::BAD_PAYLOAD));
        return RoomResult::BAD_PAYLOAD;
    }

    AcquireSRWLockExclusive(&m_lock);
    uint32_t sid = m_sessionByCtx[c];
    if (m_roomBySession[sid] != 0) {
        uint32_t currentRid = m_roomBySession[sid];
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] CREATE sid=%u result=%s currentRid=%u\n",
            sid, RoomResultToString(RoomResult::ALREADY_IN_ROOM), currentRid);

        m_net.SendRoomCreateRes(c, RoomResult::ALREADY_IN_ROOM, nullptr);
        return RoomResult::ALREADY_IN_ROOM;
    }

    Room r;
    r.id = m_nextRoomId++;
    r.title.assign(payload + 1, titleLen);
    r.members.push_back(sid);

    // 방장 설정 및 레디 (방장은 기본 레디 취급)
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
        sid, rid, createdTitle.c_str(), hostSid, RoomResultToString(RoomResult::OK));

    m_net.SendRoomCreateRes(c, RoomResult::OK, &view);
    BroadcastRoomList();
    BroadcastRoomMemberList(rid);
    return RoomResult::OK;
}

RoomResult LobbyService::HandleRoomJoinReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    uint32_t rid = 0;
    uint32_t sid = 0;
    RoomResult result = RoomResult::OK;
    RoomInfoView view{};
    bool hasView = false;
    bool shouldBroadcast = false;

    // 1) payload 파싱 실패 -> 즉시 실패 응답
    if (!ReadU32(payload, payloadLen, rid)) {
        printf("[ROOM] JOIN result=%s (bad payload)\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));
        m_net.SendRoomJoinRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    AcquireSRWLockExclusive(&m_lock);

    // 방어 코드: 세션 매핑이 없으면 비정상 요청으로 처리
    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end()) {
        ReleaseSRWLockExclusive(&m_lock);
        m_net.SendRoomJoinRes(c, RoomResult::BAD_PAYLOAD, nullptr);
        return RoomResult::BAD_PAYLOAD;
    }

    sid = itSession->second;

    // 2) 이미 다른 방에 들어가 있는지 검사
    auto itMyRoom = m_roomBySession.find(sid);
    if (itMyRoom != m_roomBySession.end() && itMyRoom->second != 0) {
        result = RoomResult::ALREADY_IN_ROOM;
    }
    else {
        // 3) 대상 방 검사
        auto itRoom = m_rooms.find(rid);
        if (itRoom == m_rooms.end()) {
            result = RoomResult::INVALID_ROOM;
        }
        else {
            Room& r = itRoom->second;

            if (r.state == RoomState::IN_GAME) {
                result = RoomResult::IN_GAME;
            }
            else if (r.members.size() >= ROOM_MAX_PLAYERS) {
                result = RoomResult::FULL;
            }
            else {
                // 4) 정상 입장
                r.members.push_back(sid);
                r.readyStatus[sid] = false; // 새로 들어온 사람은 기본 not ready
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
        sid, rid, RoomResultToString(result));

    if (hasView) {
        printf(" roomState=%s players=%u/%u host=%u title=\"%.*s\"",
            RoomStateToString(view.state),
            view.curPlayers,
            view.maxPlayers,
            view.hostId,
            view.titleLen,
            view.title);
    }
    printf("\n");

    // 5) 성공/실패 상관없이 요청자에게 응답
    m_net.SendRoomJoinRes(c, result, hasView ? &view : nullptr);

    // 6) 실제 상태가 바뀐 성공 케이스만 전체 목록 갱신
    if (shouldBroadcast) {
        BroadcastRoomList();
        BroadcastRoomMemberList(rid);
    }

    return result;
}

RoomResult LobbyService::HandleRoomLeaveReq(ClientContext* c) {
    RoomResult result = RoomResult::BAD_PAYLOAD;
    bool shouldBroadcast = false;
    uint32_t sid = 0;
    uint32_t oldRid = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession != m_sessionByCtx.end()) {
        sid = itSession->second;

        auto itRoomBySession = m_roomBySession.find(sid);
        if (itRoomBySession != m_roomBySession.end()) {
            oldRid = itRoomBySession->second;
        }

        result = LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
    }

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] LEAVE sid=%u rid=%u result=%s\n",
        sid, oldRid, RoomResultToString(result));

    if (result == RoomResult::OK) {
        m_net.SendRoomLeaveRes(c, RoomResult::OK);
    }

    if (shouldBroadcast) {
        BroadcastRoomList();

        if (oldRid != 0) {
            BroadcastRoomMemberList(oldRid);
        }
    }

    return result;
}

// ===========================================================================
// Room Actions
// ===========================================================================
void LobbyService::HandleRoomReadyReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    if (payloadLen < 1) return;
    bool isReady = (payload[0] != 0);

    AcquireSRWLockExclusive(&m_lock);
    uint32_t sid = m_sessionByCtx[c];
    uint32_t rid = m_roomBySession[sid];
    if (rid == 0) {
        printf("[ROOM] READY sid=%u ignored (NOT_IN_ROOM)\n", sid);
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    Room& r = m_rooms[rid];
    if (r.state == RoomState::IN_GAME || r.hostId == sid) {
        printf("[ROOM] READY sid=%u rid=%u ignored (IN_GAME or HOST)\n", sid, rid);
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    r.readyStatus[sid] = isReady;
    std::vector<uint32_t> membersCopy = r.members;

    printf("[ROOM] READY sid=%u rid=%u value=%d\n",
        sid, rid, isReady ? 1 : 0);

    ReleaseSRWLockExclusive(&m_lock);

    // 같은 방 사람들에게 "누가 레디했대" 방송
    printf("[ROOM] READY_BROADCAST rid=%u members=%zu fromSid=%u value=%d\n",
        rid, membersCopy.size(), sid, isReady ? 1 : 0);

    AcquireSRWLockShared(&m_lock);
    for (uint32_t mSid : membersCopy) {
        if (m_ctxBySession.count(mSid)) {
            m_net.SendRoomReadyBrd(m_ctxBySession[mSid], sid, isReady);
        }
    }
    ReleaseSRWLockShared(&m_lock);
    BroadcastRoomMemberList(rid);
}

void LobbyService::HandleRoomStartReq(ClientContext* c) {
    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession == m_sessionByCtx.end()) {
<<<<<<< Updated upstream
        ReleaseSRWLockExclusive(&m_lock);
        printf("[ROOM] START result=%s (no session)\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));
        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

    uint32_t sid = itSession->second;
    uint32_t rid = m_roomBySession[sid];

    if (rid == 0) {
        printf("[ROOM] START sid=%u result=%s\n",
            sid, RoomResultToString(RoomResult::NOT_IN_ROOM));
        ReleaseSRWLockExclusive(&m_lock);
        m_net.SendRoomStartRes(c, RoomResult::NOT_IN_ROOM);
        return;
    }

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end()) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid, rid, RoomResultToString(RoomResult::INVALID_ROOM));
        m_net.SendRoomStartRes(c, RoomResult::INVALID_ROOM);
        return;
    }

    Room& r = itRoom->second;

    // 이미 게임 시작된 방이면 Dedicated Server를 또 띄우면 안 됨.
    if (r.state == RoomState::IN_GAME) {
        uint16_t existingPort = r.dedicatedPort;

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u ignored result=%s existingPort=%u\n",
            sid, rid, RoomResultToString(RoomResult::IN_GAME), existingPort);

        m_net.SendRoomStartRes(c, RoomResult::IN_GAME);
        return;
    }

    // 1. 방장만 누를 수 있음
=======
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START result=%s (no session)\n",
            RoomResultToString(RoomResult::BAD_PAYLOAD));

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

    uint32_t sid = itSession->second;

    auto itRoomBySession = m_roomBySession.find(sid);
    if (itRoomBySession == m_roomBySession.end() || itRoomBySession->second == 0) {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u result=%s\n",
            sid, RoomResultToString(RoomResult::NOT_IN_ROOM));

        m_net.SendRoomStartRes(c, RoomResult::NOT_IN_ROOM);
        return;
    }

    uint32_t rid = itRoomBySession->second;

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end()) {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid, rid, RoomResultToString(RoomResult::INVALID_ROOM));

        m_net.SendRoomStartRes(c, RoomResult::INVALID_ROOM);
        return;
    }

    Room& r = itRoom->second;

    // 이미 게임 시작된 방이면 Dedicated Server를 또 띄우면 안 됨.
    if (r.state == RoomState::IN_GAME) {
        uint16_t existingPort = r.dedicatedPort;

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u ignored result=%s existingPort=%u\n",
            sid, rid, RoomResultToString(RoomResult::IN_GAME), existingPort);

        m_net.SendRoomStartRes(c, RoomResult::IN_GAME);
        return;
    }

    // 1. 방장만 시작 가능
>>>>>>> Stashed changes
    if (r.hostId != sid) {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid, rid, RoomResultToString(RoomResult::NOT_HOST));

        m_net.SendRoomStartRes(c, RoomResult::NOT_HOST);
        return;
    }

    // 2. 최소 2명 필요
    if (r.members.size() < 2) {
        size_t memberCount = r.members.size();

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u players=%zu result=%s\n",
            sid, rid, memberCount, RoomResultToString(RoomResult::NEED_MORE_PLAYERS));

        m_net.SendRoomStartRes(c, RoomResult::NEED_MORE_PLAYERS);
        return;
    }

<<<<<<< Updated upstream
    // 3. 다 레디 했는지 확인
=======
    // 3. 방장을 제외한 모든 인원이 레디했는지 확인
>>>>>>> Stashed changes
    for (uint32_t mSid : r.members) {
        if (mSid != r.hostId && !r.readyStatus[mSid]) {
            ReleaseSRWLockExclusive(&m_lock);

            printf("[ROOM] START sid=%u rid=%u blockerSid=%u result=%s\n",
                sid, rid, mSid, RoomResultToString(RoomResult::NOT_ALL_READY));

            m_net.SendRoomStartRes(c, RoomResult::NOT_ALL_READY);
            return;
        }
    }

    uint16_t port = AllocPort();
    if (port == 0) {
        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=NO_FREE_PORT\n",
            sid, rid);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        return;
    }

<<<<<<< Updated upstream
    // 현재 방 멤버 목록 복사
    std::vector<uint32_t> membersCopy = r.members;

    // 중요:
    // Dedicated Server 실행은 락 밖에서 하지만,
    // 그 전에 방 상태를 IN_GAME으로 예약해 중복 START 요청을 막는다.
=======
    // 현재 방 멤버 목록 복사.
    // Dedi 실행은 락 밖에서 하므로, 그 전에 복사해둔다.
    std::vector<uint32_t> membersCopy = r.members;

    // 중요:
    // Dedi 실행 전에 먼저 IN_GAME으로 예약한다.
    // 그래야 Start 패킷이 연속으로 들어와도 7777, 7778 서버가 중복 실행되지 않는다.
>>>>>>> Stashed changes
    r.state = RoomState::IN_GAME;
    r.dedicatedPort = port;

    ReleaseSRWLockExclusive(&m_lock);

    // Dedi 서버 실행
    if (!LaunchDedicatedServer(port)) {
        AcquireSRWLockExclusive(&m_lock);

        auto itRollbackRoom = m_rooms.find(rid);
        if (itRollbackRoom != m_rooms.end()) {
            itRollbackRoom->second.state = RoomState::WAITING;
            itRollbackRoom->second.dedicatedPort = 0;
        }

        FreePort(port);

        ReleaseSRWLockExclusive(&m_lock);

        printf("[ROOM] START sid=%u rid=%u result=DEDI_LAUNCH_FAILED port=%u\n",
            sid, rid, port);

        m_net.SendRoomStartRes(c, RoomResult::BAD_PAYLOAD);
        BroadcastRoomList();
        return;
    }

    printf("[ROOM] START sid=%u rid=%u result=%s port=%u members=%zu\n",
        sid, rid, RoomResultToString(RoomResult::OK), port, membersCopy.size());

    // 방장에게 시작 성공 알림
    m_net.SendRoomStartRes(c, RoomResult::OK);

    // 방 전체 인원에게 Dedi 서버 접속 정보 전송
    AcquireSRWLockShared(&m_lock);

    for (uint32_t mSid : membersCopy) {
        auto itCtx = m_ctxBySession.find(mSid);

        if (itCtx != m_ctxBySession.end()) {
            printf("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u\n",
                rid, mSid, DEDI_PUBLIC_IP, port);

            m_net.SendGameStart(itCtx->second, DEDI_PUBLIC_IP, port, 0);
        }
    }

    ReleaseSRWLockShared(&m_lock);

    BroadcastRoomList();
}


// ===========================================================================
// Helper Functions
// ===========================================================================
void LobbyService::SeedRoomsForTest_Unsafe() {
    for (int i = 0; i < 2; ++i) {
        Room r;
        r.id = m_nextRoomId++;
        r.title = "Dopamine Test " + std::to_string(r.id);

        // 더미 호스트 지정
        r.hostId = 999;

        uint32_t rid = r.id;
        m_rooms[rid] = std::move(r);
        m_roomOrder.push_back(rid);
    }
}

RoomInfoView LobbyService::BuildRoomView_Unsafe(const Room& room) const {
    RoomInfoView v{};
    v.roomId = room.id;
    v.state = room.state;
    v.curPlayers = (uint8_t)room.members.size();
    v.maxPlayers = ROOM_MAX_PLAYERS;
    v.hostId = room.hostId; // 방장 정보 세팅

    v.titleLen = (uint8_t)(std::min)((size_t)ROOM_TITLE_MAX, room.title.size());
    if (v.titleLen > 0) {
        std::memcpy(v.title, room.title.data(), v.titleLen);
    }
    return v;
}

RoomResult LobbyService::LeaveRoomInternal_Unsafe(uint32_t sid, bool& shouldBroadcast) {
    shouldBroadcast = false;

    auto itRoomBySession = m_roomBySession.find(sid);
    if (itRoomBySession == m_roomBySession.end()) {
        return RoomResult::BAD_PAYLOAD;
    }

    uint32_t rid = itRoomBySession->second;
    if (rid == 0) {
        return RoomResult::NOT_IN_ROOM;
    }

    auto itRoom = m_rooms.find(rid);
    if (itRoom == m_rooms.end()) {
        m_roomBySession[sid] = 0;
        return RoomResult::INVALID_ROOM;
    }

    Room& r = itRoom->second;

    for (auto it = r.members.begin(); it != r.members.end(); ++it) {
        if (*it == sid) {
            r.members.erase(it);
            break;
        }
    }

    m_roomBySession[sid] = 0;
    r.readyStatus.erase(sid);

    // 방장 승계
    if (r.hostId == sid) {
        if (!r.members.empty()) {
            uint32_t oldHost = sid;
            r.hostId = r.members.front();
            r.readyStatus[r.hostId] = true;

            printf("[ROOM] HOST_CHANGE rid=%u oldHost=%u newHost=%u\n",
                rid, oldHost, r.hostId);
        }
        else {
            r.hostId = 0;
        }
    }

    // 방이 비면 포트 반납 및 상태 정리
    if (r.members.empty()) {
        uint16_t oldPort = r.dedicatedPort;

        if (r.dedicatedPort != 0) {
            FreePort(r.dedicatedPort);
            r.dedicatedPort = 0;
        }
        r.state = RoomState::WAITING;

        printf("[ROOM] EMPTY rid=%u freePort=%u state=%s\n",
            rid, oldPort, RoomStateToString(RoomState::WAITING));
    }

    shouldBroadcast = true;
    return RoomResult::OK;
}

std::vector<RoomInfoView> LobbyService::BuildRoomListView_Unsafe() const {
    std::vector<RoomInfoView> views;
    for (uint32_t rid : m_roomOrder) {
        views.push_back(BuildRoomView_Unsafe(m_rooms.at(rid)));
    }
    return views;
}

void LobbyService::BroadcastRoomList() {
    std::vector<RoomInfoView> rooms;
    std::vector<ClientContext*> targets;

    AcquireSRWLockShared(&m_lock);
    rooms = BuildRoomListView_Unsafe();

    for (const auto& kv : m_sessionByCtx) {
        ClientContext* ctx = kv.first;
        uint32_t sid = kv.second;

        if (m_accountIdBySid.find(sid) == m_accountIdBySid.end()) {
            continue;
        }

        printf("[ROOM] BROADCAST_TARGET sid=%u\n", sid);

        targets.push_back(ctx);
        AddIO(ctx);
    }
    ReleaseSRWLockShared(&m_lock);

    printf("[ROOM] BROADCAST_ROOM_LIST targets=%zu rooms=%zu\n",
        targets.size(), rooms.size());

    for (auto* ctx : targets) {
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
    if (itRoom == m_rooms.end()) {
        ReleaseSRWLockShared(&m_lock);
        return;
    }

    const Room& room = itRoom->second;
    members = BuildRoomMemberListView_Unsafe(room);

    for (uint32_t sid : room.members) {
        auto itCtx = m_ctxBySession.find(sid);
        if (itCtx != m_ctxBySession.end()) {
            ClientContext* ctx = itCtx->second;
            targets.push_back(ctx);
            AddIO(ctx);
        }
    }

    ReleaseSRWLockShared(&m_lock);

    printf("[ROOM] BROADCAST_MEMBER_LIST rid=%u targets=%zu members=%zu\n",
        roomId, targets.size(), members.size());

    for (ClientContext* ctx : targets) {
        m_net.SendRoomMemberList(ctx, roomId, members);
        ReleaseIO(ctx);
    }
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
        if (itNick != m_nicknameBySid.end()) {
            nickname = itNick->second;
        }
        else {
            auto itAccount = m_accountIdBySid.find(sid);
            if (itAccount != m_accountIdBySid.end()) {
                nickname = itAccount->second;
            }
            else {
                nickname = "Player" + std::to_string(sid);
            }
        }

        v.nicknameLen = static_cast<uint8_t>((std::min)(
            nickname.size(),
            static_cast<size_t>(MAX_NICKNAME_LEN)
            ));

        if (v.nicknameLen > 0) {
            std::memcpy(v.nickname, nickname.data(), v.nicknameLen);
        }

        views.push_back(v);
    }

    return views;
}

bool LobbyService::ReadU32(const char* payload, uint16_t payloadLen, uint32_t& outHost) {
    if (!payload || payloadLen < 4) return false;
    uint32_t net;
    std::memcpy(&net, payload, 4);
    outHost = ntohl(net);
    return true;
}