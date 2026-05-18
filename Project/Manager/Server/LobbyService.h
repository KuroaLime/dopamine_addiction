#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include "Protocol.h"
#include "NetApi.h"

struct ClientContext;

class LobbyService
{
public:
    // =======================================================================
    // Public Interface (Lifecycle & Events)
    // =======================================================================
    explicit LobbyService(NetApi& net);

    // 클라이언트 접속/종료 이벤트
    void OnClientAccepted(ClientContext* c);
    void OnClientDisconnected(ClientContext* c);

    // 패킷 수신 이벤트
    void OnPacket(ClientContext* c, uint16_t type, const char* payload, uint16_t payloadLen);


private:
    // =======================================================================
    // Internal Data Structures
    // =======================================================================
    struct Room {
        uint32_t id = 0;
        std::string title;
        RoomState state = RoomState::WAITING;
        std::vector<uint32_t> members; // Session IDs

        uint32_t hostId = 0;                               // 방장의 Session ID
        std::unordered_map<uint32_t, bool> readyStatus;    // Session ID -> Ready 상태 (true/false)

        // Dedicated Server 연동 정보
        uint16_t dedicatedPort = 0;    // 0 = None, Others = Assigned Port
    };


    // =======================================================================
    // [SECTION 3] Member Variables
    // =======================================================================

    // 1. System & Synchronization
    NetApi& m_net;
    SRWLOCK m_lock;

    // 2. ID Generators
    uint32_t m_nextSessionId = 1;
    uint32_t m_nextRoomId = 1;

    // 3. Dedicated Server Port Pool
    std::vector<uint16_t> m_freePorts;

    // 4. Lookup Containers
    std::unordered_map<ClientContext*, uint32_t> m_sessionByCtx; // ContextPtr -> SessionID
    std::unordered_map<uint32_t, ClientContext*> m_ctxBySession; // SessionID -> ContextPtr

    std::unordered_map<uint32_t, uint32_t> m_roomBySession;      // SessionID -> RoomID (0 if none)
    std::unordered_map<uint32_t, Room>     m_rooms;              // RoomID -> Room Object
    std::vector<uint32_t>                  m_roomOrder;          // Room List Order

    std::unordered_map<std::string, std::string> m_userDB;          // Account ID -> Password (회원가입 정보)
    std::unordered_map<uint32_t, std::string>    m_accountIdBySid;  // Session ID -> Account ID (현재 접속자 추적)


    // =======================================================================
    // Internal Logic Methods
    // =======================================================================

    // 1. Port Pool Management (Alloc/Free)
    void     InitPortPool(uint16_t start, int count);
    uint16_t AllocPort();
    void     FreePort(uint16_t port);

    // Dedicated Server Process
    bool     LaunchDedicatedServer(uint16_t port);

    // 2. Helper Functions (Room Views & Broadcast)
    void     SeedRoomsForTest_Unsafe();
    void     BroadcastRoomList();

    // View 생성 헬퍼
    RoomInfoView              BuildRoomView_Unsafe(const Room& room) const;
    std::vector<RoomInfoView> BuildRoomListView_Unsafe() const;

    // 3. Packet Handlers
    void       HandleLoginReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRegisterReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRoomReadyReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    void       HandleRoomStartReq(ClientContext* c);
    
    // 3. Packet Handlers
    RoomResult HandleRoomListReq(ClientContext* c);
    RoomResult HandleRoomCreateReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    RoomResult HandleRoomJoinReq(ClientContext* c, const char* payload, uint16_t payloadLen);
    RoomResult HandleRoomLeaveReq(ClientContext* c);

    RoomResult LeaveRoomInternal_Unsafe(uint32_t sid, bool& shouldBroadcast);

    // 4. Static Utils
    static bool ReadU32(const char* payload, uint16_t payloadLen, uint32_t& outHost);

    static bool ParseAuthPayload(const char* payload, uint16_t payloadLen, std::string& outId, std::string& outPw);
};