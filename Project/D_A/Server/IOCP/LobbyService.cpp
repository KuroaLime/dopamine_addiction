// LobbyService.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LobbyService.h"
#include <cstring>
#include <algorithm> 
#include <cstdio>    

#ifdef _WIN32
#include <winsock2.h>
#endif

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
bool LobbyService::ParseAuthPayload(const char* payload, uint16_t payloadLen, std::string& outId, std::string& outPw) {
    if (payloadLen < 1) return false;
    uint8_t idLen = payload[0];
    if (payloadLen < 1 + idLen + 1) return false;
    outId.assign(payload + 1, idLen);

    uint8_t pwLen = payload[1 + idLen];
    if (payloadLen < 1 + idLen + 1 + pwLen) return false;
    outPw.assign(payload + 1 + idLen + 1, pwLen);
    return true;
}

void LobbyService::HandleRegisterReq(ClientContext* c, const char* payload, uint16_t payloadLen) {
    std::string id, pw;
    if (!ParseAuthPayload(payload, payloadLen, id, pw)) {
        printf("[AUTH] REGISTER result=%s\n", LoginResultToString(LoginResult::INVALID_FORMAT));
        m_net.SendRegisterRes(c, LoginResult::INVALID_FORMAT);
        return;
    }
    AcquireSRWLockExclusive(&m_lock);
    if (m_userDB.find(id) != m_userDB.end()) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[AUTH] REGISTER id=%s result=%s\n",
            id.c_str(), LoginResultToString(LoginResult::ID_ALREADY_EXISTS));
        m_net.SendRegisterRes(c, LoginResult::ID_ALREADY_EXISTS);
        return;
    }
    m_userDB[id] = pw;
    ReleaseSRWLockExclusive(&m_lock);

    printf("[AUTH] REGISTER id=%s result=%s\n",
        id.c_str(), LoginResultToString(LoginResult::OK));

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
    if (it->second != pw) {
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

        m_sessionByCtx[c] = oldSid;
        m_ctxBySession[oldSid] = c;

        uint32_t rid = m_roomBySession[oldSid];
        if (rid != 0 && m_rooms.count(rid) > 0) {
            Room& r = m_rooms[rid];
            if (r.state == RoomState::IN_GAME) {
                ReleaseSRWLockExclusive(&m_lock);

                printf("[AUTH] LOGIN id=%s sid=%u result=%s port=%u\n",
                    id.c_str(), oldSid, LoginResultToString(LoginResult::OK_RECONNECT), r.dedicatedPort);

                m_net.SendLoginRes(c, LoginResult::OK_RECONNECT);
                m_net.SendGameStart(c, "127.0.0.1", r.dedicatedPort, 0);
                return;
            }
        }
    }
    else {
        // 완전 첫 로그인
        m_accountIdBySid[m_sessionByCtx[c]] = id;
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
    }

    return result;
}

RoomResult LobbyService::HandleRoomLeaveReq(ClientContext* c) {
    RoomResult result = RoomResult::BAD_PAYLOAD;
    bool shouldBroadcast = false;
    uint32_t sid = 0;

    AcquireSRWLockExclusive(&m_lock);

    auto itSession = m_sessionByCtx.find(c);
    if (itSession != m_sessionByCtx.end()) {
        sid = itSession->second;
        result = LeaveRoomInternal_Unsafe(sid, shouldBroadcast);
    }

    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] LEAVE sid=%u result=%s\n",
        sid, RoomResultToString(result));

    if (result == RoomResult::OK) {
        m_net.SendRoomLeaveRes(c, RoomResult::OK);
    }

    if (shouldBroadcast) {
        BroadcastRoomList();
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
}

void LobbyService::HandleRoomStartReq(ClientContext* c) {
    AcquireSRWLockExclusive(&m_lock);
    uint32_t sid = m_sessionByCtx[c];
    uint32_t rid = m_roomBySession[sid];
    if (rid == 0) {
        printf("[ROOM] START sid=%u result=%s\n",
            sid, RoomResultToString(RoomResult::NOT_IN_ROOM));
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    Room& r = m_rooms[rid];
    // 1. 방장만 누를 수 있음
    if (r.hostId != sid) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[ROOM] START sid=%u rid=%u result=%s\n",
            sid, rid, RoomResultToString(RoomResult::NOT_HOST));
        m_net.SendRoomStartRes(c, RoomResult::NOT_HOST);
        return;
    }
    // 2. 최소 2명 필요
    if (r.members.size() < 2) {
        ReleaseSRWLockExclusive(&m_lock);
        printf("[ROOM] START sid=%u rid=%u players=%zu result=%s\n",
            sid, rid, r.members.size(), RoomResultToString(RoomResult::NEED_MORE_PLAYERS));
        m_net.SendRoomStartRes(c, RoomResult::NEED_MORE_PLAYERS);
        return;
    }
    // 3. 다 레디 했는지 확인
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
        printf("[ROOM] START sid=%u rid=%u result=NO_FREE_PORT\n", sid, rid);
        return;
    }

    r.state = RoomState::IN_GAME;
    r.dedicatedPort = port;
    std::vector<uint32_t> membersCopy = r.members;
    ReleaseSRWLockExclusive(&m_lock);

    printf("[ROOM] START sid=%u rid=%u result=%s port=%u members=%zu\n",
        sid, rid, RoomResultToString(RoomResult::OK), port, membersCopy.size());

    // 방장에게 성공 알림
    m_net.SendRoomStartRes(c, RoomResult::OK);

    // 전원에게 게임 서버 이동 패킷 전송
    AcquireSRWLockShared(&m_lock);
    for (uint32_t mSid : membersCopy) {
        if (m_ctxBySession.count(mSid)) {
            printf("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u\n",
                rid, mSid, "127.0.0.1", port);
            m_net.SendGameStart(m_ctxBySession[mSid], "127.0.0.1", port, 0);
        }
    }
    ReleaseSRWLockShared(&m_lock);

    BroadcastRoomList(); // 방이 게임 중으로 변했음을 전체 알림
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

bool LobbyService::ReadU32(const char* payload, uint16_t payloadLen, uint32_t& outHost) {
    if (!payload || payloadLen < 4) return false;
    uint32_t net;
    std::memcpy(&net, payload, 4);
    outHost = ntohl(net);
    return true;
}