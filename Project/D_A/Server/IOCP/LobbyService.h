#pragma once
// LobbyService.h
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "NetApi.h"
#include "Protocol.h"

struct ClientContext;

class LobbyService
{
public:
    explicit LobbyService(NetApi& net);

    void OnClientConnected(ClientContext* c);
    void OnClientDisconnected(ClientContext* c);

    // 네 ServerMain의 DispatchPacket에서 호출
    // return false면 "이 패킷은 잘못된 요청" -> ServerMain이 BeginClose 하게 만들기
    bool OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen);

    // 방 목록 보내기(접속 직후, 혹은 요청 시)
    void SendRoomList(ClientContext* c);

private:
    struct SessionInfo
    {
        uint32_t sessionId = 0;
    };

    struct Room
    {
        uint32_t roomId = 0;
        RoomState state = RoomState::Waiting;
        uint8_t curPlayers = 0;
        uint8_t maxPlayers = 4;
        uint32_t dediIp = 0;     // network order IPv4
        uint16_t dediPort = 0;   // network order
    };

private:
    void SendWelcome(ClientContext* c, uint32_t sessionId);

private:
    NetApi& net_;
    std::atomic<uint32_t> nextSessionId_{ 1 };

    std::mutex m_;
    std::unordered_map<ClientContext*, SessionInfo> sessions_;
    std::vector<Room> rooms_;
};
