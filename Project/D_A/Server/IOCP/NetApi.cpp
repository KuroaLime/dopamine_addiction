#include "NetApi.h"
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#endif

// ===========================================================================
// Serialization Helpers (데이터 직렬화 도구)
// ===========================================================================

void NetApi::AppendU8(std::vector<char>& out, uint8_t v) {
    out.push_back(static_cast<char>(v));
}

void NetApi::AppendU16(std::vector<char>& out, uint16_t vNet) {
    char b[2];
    std::memcpy(b, &vNet, 2);
    out.insert(out.end(), b, b + 2);
}

void NetApi::AppendU32(std::vector<char>& out, uint32_t vNet) {
    char b[4];
    std::memcpy(b, &vNet, 4);
    out.insert(out.end(), b, b + 4);
}


// ===========================================================================
// Packet Senders (패킷 전송 함수)
// ===========================================================================

// ---------------------------------------------------------------------------
// [System] Welcome & Pong
// ---------------------------------------------------------------------------
void NetApi::SendWelcome(ClientContext* c, uint32_t sessionId) {
    // [Payload] SessionID
    uint32_t netSid = htonl(sessionId);

    // [Send]
    m_send(c, (uint16_t)PacketType::S2C_WELCOME, &netSid, 4);
}

void NetApi::SendPong(ClientContext* c) {
    // [Payload] None
    // [Send]
    m_send(c, (uint16_t)PacketType::S2C_PONG, nullptr, 0);
}

// ---------------------------------------------------------------------------
// [Auth] 로그인/회원가입 결과
// ---------------------------------------------------------------------------
void NetApi::SendLoginRes(ClientContext* c, LoginResult result) {
    uint8_t r = (uint8_t)result;
    m_send(c, (uint16_t)PacketType::S2C_LOGIN_RES, &r, 1);
}

void NetApi::SendRegisterRes(ClientContext* c, LoginResult result) {
    uint8_t r = (uint8_t)result;
    m_send(c, (uint16_t)PacketType::S2C_REGISTER_RES, &r, 1);
}


// ---------------------------------------------------------------------------
// [Lobby] Room List Response
// ---------------------------------------------------------------------------
void NetApi::SendRoomListRes(ClientContext* c, const std::vector<RoomInfoView>& rooms) {
    std::vector<char> payload;

    // 메모리 예약: (Count 2바이트) + (방 개수 * 대략적인 방 정보 크기 64바이트)
    payload.reserve(2 + rooms.size() * 64);

    // 1. 방 개수
    AppendU16(payload, htons((uint16_t)rooms.size()));

    // 2. 각 방 정보 반복
    for (const auto& r : rooms) {
        AppendU32(payload, htonl(r.roomId));
        AppendU8(payload, (uint8_t)r.state);
        AppendU8(payload, r.curPlayers);
        AppendU8(payload, r.maxPlayers);

        AppendU32(payload, htonl(r.hostId));

        // 가변 길이 문자열 처리
        AppendU8(payload, r.titleLen);
        if (r.titleLen > 0) {
            payload.insert(payload.end(), r.title, r.title + r.titleLen);
        }
    }

    // [Send]
    m_send(c, (uint16_t)PacketType::S2C_ROOM_LIST_RES, payload.data(), (uint16_t)payload.size());
}


// ---------------------------------------------------------------------------
// [Room] Create & Join Response
// ---------------------------------------------------------------------------
void NetApi::SendRoomCreateRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull) {
    std::vector<char> payload;

    // 1. 결과 코드
    AppendU8(payload, (uint8_t)result);

    // 2. 성공 시, 생성된 방 정보 포함
    if (result == RoomResult::OK && roomOrNull) {
        AppendU32(payload, htonl(roomOrNull->roomId));
        AppendU8(payload, (uint8_t)roomOrNull->state);
        AppendU8(payload, roomOrNull->curPlayers);
        AppendU8(payload, roomOrNull->maxPlayers);

        AppendU32(payload, htonl(roomOrNull->hostId));

        AppendU8(payload, roomOrNull->titleLen);
        if (roomOrNull->titleLen > 0) {
            payload.insert(payload.end(), roomOrNull->title, roomOrNull->title + roomOrNull->titleLen);
        }
    }

    m_send(c, (uint16_t)PacketType::S2C_ROOM_CREATE_RES, payload.data(), (uint16_t)payload.size());
}

void NetApi::SendRoomJoinRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull) {
    std::vector<char> payload;

    // 1. 결과 코드
    AppendU8(payload, (uint8_t)result);

    // 2. 성공 시, 입장한 방 정보 포함
    if (result == RoomResult::OK && roomOrNull) {
        AppendU32(payload, htonl(roomOrNull->roomId));
        AppendU8(payload, (uint8_t)roomOrNull->state);
        AppendU8(payload, roomOrNull->curPlayers);
        AppendU8(payload, roomOrNull->maxPlayers);

        AppendU32(payload, htonl(roomOrNull->hostId));

        AppendU8(payload, roomOrNull->titleLen);
        if (roomOrNull->titleLen > 0) {
            payload.insert(payload.end(), roomOrNull->title, roomOrNull->title + roomOrNull->titleLen);
        }
    }

    m_send(c, (uint16_t)PacketType::S2C_ROOM_JOIN_RES, payload.data(), (uint16_t)payload.size());
}


// ---------------------------------------------------------------------------
// [Room] Leave Response
// ---------------------------------------------------------------------------
void NetApi::SendRoomLeaveRes(ClientContext* c, RoomResult result) {
    uint8_t r = (uint8_t)result;
    m_send(c, (uint16_t)PacketType::S2C_ROOM_LEAVE_RES, &r, 1);
}

// ---------------------------------------------------------------------------
// [Room Action] 레디 브로드캐스트 / 시작 결과
// ---------------------------------------------------------------------------
void NetApi::SendRoomReadyBrd(ClientContext* c, uint32_t sessionId, bool isReady) {
    std::vector<char> payload;
    AppendU32(payload, htonl(sessionId));       // 누가?
    AppendU8(payload, isReady ? 1 : 0);         // 레디했나? 취소했나?

    m_send(c, (uint16_t)PacketType::S2C_ROOM_READY_BRD, payload.data(), (uint16_t)payload.size());
}

void NetApi::SendRoomStartRes(ClientContext* c, RoomResult result) {
    uint8_t r = (uint8_t)result;
    m_send(c, (uint16_t)PacketType::S2C_ROOM_START_RES, &r, 1);
}


// ---------------------------------------------------------------------------
// Game Start (Handover)
// ---------------------------------------------------------------------------
void NetApi::SendGameStart(ClientContext* c, const char* ip, uint16_t port, uint32_t ticket)
{
    std::vector<char> payload;

    // IP 문자열 길이 계산 및 안전장치
    size_t len = 0;
    if (ip != nullptr) {
        len = std::strlen(ip);
        if (len > 255) len = 255; // uint8_t 범위 제한
    }

    // Payload 구성
    // 1. IP Length & String
    AppendU8(payload, static_cast<uint8_t>(len));
    if (len > 0) {
        payload.insert(payload.end(), ip, ip + len);
    }

    // 2. 보안 토큰
    AppendU32(payload, htonl(ticket));

    // 3. Dedicated Server Port
    AppendU16(payload, htons(port));

    // Send
    m_send(c, static_cast<uint16_t>(PacketType::S2C_GAME_START), payload.data(), static_cast<uint16_t>(payload.size()));
}