// LobbyService.cpp
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "LobbyService.h"

LobbyService::LobbyService(NetApi& net) : net_(net)
{
    
}

void LobbyService::OnClientConnected(ClientContext* c)
{
    const uint32_t sid = nextSessionId_.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(m_);
        sessions_[c].sessionId = sid;
    }

    SendWelcome(c, sid);
    SendRoomList(c);
}

void LobbyService::OnClientDisconnected(ClientContext* c)
{
    std::lock_guard<std::mutex> lock(m_);
    sessions_.erase(c);

}

bool LobbyService::OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen)
{
    (void)payload;
    (void)payloadLen;

    const auto pt = static_cast<PacketType>(type);

    switch (pt)
    {
    case PacketType::C2S_PING:
        // PONG은 payload 없음
        if (net_.SendPacket)
            net_.SendPacket(c, static_cast<uint16_t>(PacketType::S2C_PONG), nullptr, 0);
        return true;

    case PacketType::C2S_ROOM_LIST_REQ:
        SendRoomList(c);
        return true;

    default:
        return false; // ServerMain이 BeginClose 하도록
    }
}

void LobbyService::SendWelcome(ClientContext* c, uint32_t sessionId)
{
    if (!net_.SendPacket) return;

    WelcomePayload p{};
    p.sessionId = htonl(sessionId);

    net_.SendPacket(c, static_cast<uint16_t>(PacketType::S2C_WELCOME), &p, static_cast<uint16_t>(sizeof(p)));
}

void LobbyService::SendRoomList(ClientContext* c)
{
    if (!net_.SendPacket) return;

    std::vector<Room> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_);
        snapshot = rooms_;
    }

    // MAX_PACKET_SIZE는 ServerMain에서 4096으로 제한 중.
    // 헤더/RoomListResHeader/RoomEntry 크기 기준으로 안전하게 잘라서 전송.
    const size_t maxPayload = 4096 - sizeof(PacketHeader);
    const size_t base = sizeof(RoomListResHeader);
    const size_t entry = sizeof(RoomEntry);

    size_t maxRooms = 0;
    if (maxPayload >= base)
        maxRooms = (maxPayload - base) / entry;

    if (snapshot.size() > maxRooms)
        snapshot.resize(maxRooms);

    const uint16_t roomCount = static_cast<uint16_t>(snapshot.size());

    std::vector<char> payload;
    payload.resize(base + entry * snapshot.size());

    RoomListResHeader h{};
    h.roomCount = htons(roomCount);
    h.reserved = 0;
    std::memcpy(payload.data(), &h, sizeof(h));

    char* out = payload.data() + sizeof(h);

    for (const auto& r : snapshot)
    {
        RoomEntry e{};
        e.roomId = htonl(r.roomId);
        e.state = static_cast<uint8_t>(r.state);
        e.curPlayers = r.curPlayers;
        e.maxPlayers = r.maxPlayers;
        e.reserved0 = 0;
        e.dediIp = r.dediIp;        // 이미 network order로 관리한다고 가정
        e.dediPort = r.dediPort;    // 이미 network order로 관리한다고 가정
        e.reserved1 = 0;

        std::memcpy(out, &e, sizeof(e));
        out += sizeof(e);
    }

    net_.SendPacket(c, static_cast<uint16_t>(PacketType::S2C_ROOM_LIST_RES),
        payload.data(), static_cast<uint16_t>(payload.size()));
}
