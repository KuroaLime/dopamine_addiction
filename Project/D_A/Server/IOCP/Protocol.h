// Protocol.h
#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
    // basic
    C2S_PING = 1,
    S2C_PONG = 2,

    // lobby #1 (스냅샷/방목록)
    S2C_WELCOME = 10, // 접속 직후 세션ID 부여
    C2S_ROOM_LIST_REQ = 11, // 방 목록 요청
    S2C_ROOM_LIST_RES = 12, // 방 목록 응답

    
    C2S_CREATE_ROOM_REQ = 20,
    S2C_CREATE_ROOM_RES = 21,
    C2S_JOIN_ROOM_REQ = 22,
    S2C_JOIN_ROOM_RES = 23,

    S2C_ERROR = 1000,
};

// 헤더(4바이트): size/type는 "네트워크 바이트 오더"로 송수신
struct PacketHeader
{
    uint16_t size; // 헤더 포함 전체 크기
    uint16_t type; // PacketType
};
static_assert(sizeof(PacketHeader) == 4, "PacketHeader size must be 4");

// S2C_WELCOME payload (4바이트)
struct WelcomePayload
{
    uint32_t sessionId; // network order (htonl)
};
static_assert(sizeof(WelcomePayload) == 4, "WelcomePayload size must be 4");

// S2C_ROOM_LIST_RES payload:
// [RoomListResHeader][RoomEntry x N]
struct RoomListResHeader
{
    uint16_t roomCount; // network order (htons)
    uint16_t reserved;  // 0
};
static_assert(sizeof(RoomListResHeader) == 4, "RoomListResHeader size must be 4");

enum class RoomState : uint8_t
{
    Waiting = 0,
    InGame = 1,
};

struct RoomEntry
{
    uint32_t roomId;      // network order (htonl)
    uint8_t  state;       // RoomState
    uint8_t  curPlayers;  // 0~4
    uint8_t  maxPlayers;  // 4
    uint8_t  reserved0;   // 0

    uint32_t dediIp;      // IPv4 network order (0이면 아직 없음)
    uint16_t dediPort;    // network order (htons), 0이면 아직 없음
    uint16_t reserved1;   // 0
};
static_assert(sizeof(RoomEntry) == 16, "RoomEntry size must be 16");
