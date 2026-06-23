#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdio>

#include "Protocol_D.h"
#include "NetApi.h"

struct ClientContext;

class LobbyService
{
public:
    // =======================================================================
    // Public Interface
    // =======================================================================
    explicit LobbyService(NetApi& net);

    void OnClientAccepted(ClientContext* c);
    void OnClientDisconnected(ClientContext* c);

    void OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen);

    void HandleDediMatchEndNotify(ClientContext* c, const char* payload, uint16_t payloadLen);


private:
    // =======================================================================
    // Internal Data Structures
    // =======================================================================

    struct Room
    {
        uint32_t id = 0;
        std::string title;
        RoomState state = RoomState::WAITING;

        std::vector<uint32_t> members; // Session IDs

        uint32_t hostId = 0;
        std::unordered_map<uint32_t, bool> readyStatus; // Session ID -> Ready

        uint16_t dedicatedPort = 0; // 0 = none
    };

    struct UserRecord
    {
        std::string password;
        std::string nickname;
    };


    // =======================================================================
    // Member Variables
    // =======================================================================

    NetApi& m_net;
    SRWLOCK m_lock;

    uint32_t m_nextSessionId = 1;
    uint32_t m_nextRoomId = 1;

    std::vector<uint16_t> m_freePorts;

    std::unordered_map<ClientContext*, uint32_t> m_sessionByCtx;
    std::unordered_map<uint32_t, ClientContext*> m_ctxBySession;

    std::unordered_map<uint32_t, uint32_t> m_roomBySession;
    std::unordered_map<uint32_t, Room>     m_rooms;
    std::vector<uint32_t>                  m_roomOrder;

    // Account ID -> UserRecord
    std::unordered_map<std::string, UserRecord> m_userDB;

    // Session ID -> Account ID / Nickname
    std::unordered_map<uint32_t, std::string> m_accountIdBySid;
    std::unordered_map<uint32_t, std::string> m_nicknameBySid;


    // =======================================================================
    // Internal Logic Methods
    // =======================================================================

    void     InitPortPool(uint16_t start, int count);
    uint16_t AllocPort();
    void     FreePort(uint16_t port);

    bool     LaunchDedicatedServer(uint16_t port, uint32_t roomId, uint16_t requiredPlayers);

    void     BroadcastRoomList();
    void     BroadcastRoomMemberList(uint32_t roomId);

    RoomInfoView BuildRoomView_Unsafe(const Room& room) const;
    std::vector<RoomInfoView> BuildRoomListView_Unsafe() const;

    std::vector<RoomMemberInfoView> BuildRoomMemberListView_Unsafe(const Room& room) const;

    void       HandleLoginReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRegisterReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRoomReadyReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRoomStartReq(ClientContext* c);

    RoomResult HandleRoomListReq(ClientContext* c);
    RoomResult HandleRoomCreateReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    RoomResult HandleRoomJoinReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    RoomResult HandleRoomLeaveReq(ClientContext* c);

    RoomResult LeaveRoomInternal_Unsafe(uint32_t sid, bool& shouldBroadcast);

    static bool ReadU32(const char* payload, uint16_t payloadLen, uint32_t& outHost);

    static bool ParseAuthPayload(
        const char* payload,
        uint16_t payloadLen,
        std::string& outId,
        std::string& outPw,
        std::string* outNickname = nullptr
    );
};