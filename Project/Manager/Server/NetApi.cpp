#include "NetApi.h"



#ifdef _WIN32
#include <winsock2.h>
#endif


// ===========================================================================
// Serialization Helpers
// ===========================================================================

void NetApi::AppendU8(std::vector<char>& out, uint8_t v)
{
    out.push_back(static_cast<char>(v));
}

void NetApi::AppendU16(std::vector<char>& out, uint16_t vNet)
{
    char b[2];
    std::memcpy(b, &vNet, 2);
    out.insert(out.end(), b, b + 2);
}

void NetApi::AppendU32(std::vector<char>& out, uint32_t vNet)
{
    char b[4];
    std::memcpy(b, &vNet, 4);
    out.insert(out.end(), b, b + 4);
}


// ===========================================================================
// Packet Senders
// ===========================================================================

// ---------------------------------------------------------------------------
// [System] Welcome & Pong
// ---------------------------------------------------------------------------

void NetApi::SendWelcome(ClientContext* c, uint32_t sessionId)
{
    uint32_t netSid = htonl(sessionId);
    m_send(c, static_cast<uint16_t>(PacketType::S2C_WELCOME), &netSid, 4);
}

void NetApi::SendPong(ClientContext* c)
{
    m_send(c, static_cast<uint16_t>(PacketType::S2C_PONG), nullptr, 0);
}


// ---------------------------------------------------------------------------
// [Auth] Login / Register Result
// ---------------------------------------------------------------------------

void NetApi::SendLoginRes(ClientContext* c, LoginResult result)
{
    uint8_t r = static_cast<uint8_t>(result);
    m_send(c, static_cast<uint16_t>(PacketType::S2C_LOGIN_RES), &r, 1);
}

void NetApi::SendRegisterRes(ClientContext* c, LoginResult result)
{
    uint8_t r = static_cast<uint8_t>(result);
    m_send(c, static_cast<uint16_t>(PacketType::S2C_REGISTER_RES), &r, 1);
}


// ---------------------------------------------------------------------------
// [Lobby] Room List Response
// ---------------------------------------------------------------------------

void NetApi::SendRoomListRes(ClientContext* c, const std::vector<RoomInfoView>& rooms)
{
    std::vector<char> payload;

    // roomCount(2) + room data
    payload.reserve(2 + rooms.size() * 64);

    // 1. roomCount
    AppendU16(payload, htons(static_cast<uint16_t>(rooms.size())));

    // 2. rooms
    for (const RoomInfoView& r : rooms)
    {
        AppendU32(payload, htonl(r.roomId));
        AppendU8(payload, static_cast<uint8_t>(r.state));
        AppendU8(payload, r.curPlayers);
        AppendU8(payload, r.maxPlayers);

        AppendU32(payload, htonl(r.hostId));

        AppendU8(payload, r.titleLen);
        if (r.titleLen > 0)
        {
            payload.insert(payload.end(), r.title, r.title + r.titleLen);
        }
    }

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_LIST_RES),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}


// ---------------------------------------------------------------------------
// [Room] Create Response
// ---------------------------------------------------------------------------

void NetApi::SendRoomCreateRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull)
{
    std::vector<char> payload;

    // 1. result
    AppendU8(payload, static_cast<uint8_t>(result));

    // 2. room info
    if (result == RoomResult::OK && roomOrNull)
    {
        AppendU32(payload, htonl(roomOrNull->roomId));
        AppendU8(payload, static_cast<uint8_t>(roomOrNull->state));
        AppendU8(payload, roomOrNull->curPlayers);
        AppendU8(payload, roomOrNull->maxPlayers);

        AppendU32(payload, htonl(roomOrNull->hostId));

        AppendU8(payload, roomOrNull->titleLen);
        if (roomOrNull->titleLen > 0)
        {
            payload.insert(
                payload.end(),
                roomOrNull->title,
                roomOrNull->title + roomOrNull->titleLen
            );
        }
    }

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_CREATE_RES),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}


// ---------------------------------------------------------------------------
// [Room] Join Response
// ---------------------------------------------------------------------------

void NetApi::SendRoomJoinRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull)
{
    std::vector<char> payload;

    // 1. result
    AppendU8(payload, static_cast<uint8_t>(result));

    // 2. room info
    if (result == RoomResult::OK && roomOrNull)
    {
        AppendU32(payload, htonl(roomOrNull->roomId));
        AppendU8(payload, static_cast<uint8_t>(roomOrNull->state));
        AppendU8(payload, roomOrNull->curPlayers);
        AppendU8(payload, roomOrNull->maxPlayers);

        AppendU32(payload, htonl(roomOrNull->hostId));

        AppendU8(payload, roomOrNull->titleLen);
        if (roomOrNull->titleLen > 0)
        {
            payload.insert(
                payload.end(),
                roomOrNull->title,
                roomOrNull->title + roomOrNull->titleLen
            );
        }
    }

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_JOIN_RES),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}


// ---------------------------------------------------------------------------
// [Room] Leave Response
// ---------------------------------------------------------------------------

void NetApi::SendRoomLeaveRes(ClientContext* c, RoomResult result)
{
    uint8_t r = static_cast<uint8_t>(result);

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_LEAVE_RES),
        &r,
        1
    );
}


// ---------------------------------------------------------------------------
// [Room] Member List
// ---------------------------------------------------------------------------

void NetApi::SendRoomMemberList(
    ClientContext* c,
    uint32_t roomId,
    const std::vector<RoomMemberInfoView>& members
)
{
    std::vector<char> payload;

    // roomId(4) + memberCount(1) + member data
    payload.reserve(
        4 + 1 + members.size() * (4 + 1 + 1 + 1 + MAX_NICKNAME_LEN)
    );

    // 1. roomId
    AppendU32(payload, htonl(roomId));

    // 2. memberCount
    uint8_t count = static_cast<uint8_t>((std::min)(
        members.size(),
        static_cast<size_t>(ROOM_MAX_PLAYERS)
        ));

    AppendU8(payload, count);

    // 3. members
    for (uint8_t i = 0; i < count; ++i)
    {
        const RoomMemberInfoView& m = members[i];

        AppendU32(payload, htonl(m.sessionId));
        AppendU8(payload, m.isHost);
        AppendU8(payload, m.isReady);

        uint8_t nicknameLen = static_cast<uint8_t>((std::min)(
            static_cast<size_t>(m.nicknameLen),
            static_cast<size_t>(MAX_NICKNAME_LEN)
            ));

        AppendU8(payload, nicknameLen);

        if (nicknameLen > 0)
        {
            payload.insert(
                payload.end(),
                m.nickname,
                m.nickname + nicknameLen
            );
        }
    }

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_MEMBER_LIST),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}


// ---------------------------------------------------------------------------
// [Room Action] Ready Broadcast / Start Result
// ---------------------------------------------------------------------------

void NetApi::SendRoomReadyBrd(ClientContext* c, uint32_t sessionId, bool isReady)
{
    std::vector<char> payload;

    AppendU32(payload, htonl(sessionId));
    AppendU8(payload, isReady ? 1 : 0);

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_READY_BRD),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}

void NetApi::SendRoomStartRes(ClientContext* c, RoomResult result)
{
    uint8_t r = static_cast<uint8_t>(result);

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_ROOM_START_RES),
        &r,
        1
    );
}


// ---------------------------------------------------------------------------
// [Game] Game Start / Dedicated Server Handover
// ---------------------------------------------------------------------------

void NetApi::SendGameStart(ClientContext* c, const char* ip, uint16_t port, uint32_t ticket)
{
    std::vector<char> payload;

    // 1. ip length & ip string
    size_t len = 0;

    if (ip != nullptr)
    {
        len = std::strlen(ip);

        if (len > 255)
        {
            len = 255;
        }
    }

    AppendU8(payload, static_cast<uint8_t>(len));

    if (len > 0)
    {
        payload.insert(payload.end(), ip, ip + len);
    }

    // 2. ticket
    AppendU32(payload, htonl(ticket));

    // 3. port
    AppendU16(payload, htons(port));

    m_send(
        c,
        static_cast<uint16_t>(PacketType::S2C_GAME_START),
        payload.data(),
        static_cast<uint16_t>(payload.size())
    );
}