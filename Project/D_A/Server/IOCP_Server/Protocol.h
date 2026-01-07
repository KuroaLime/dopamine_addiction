// Protocol.h
#pragma once
#include <cstdint>

// 패킷 타입
enum PacketType : uint16_t
{
    PKT_C2S_PING = 1,
    PKT_S2C_PONG = 2,
};

// 헤더는 2바이트+2바이트
struct PacketHeader
{
    uint16_t size; // 헤더 포함 전체 크기
    uint16_t type; // PacketType
};
static_assert(sizeof(PacketHeader) == 4, "PacketHeader size must be 4");

// 패킷
struct C2S_Ping
{
    PacketHeader h;
    uint32_t clientTick;
};
static_assert(sizeof(C2S_Ping) == 8, "C2S_Ping size must be 8");

struct S2C_Pong
{
    PacketHeader h;
    uint32_t serverTick;
};
static_assert(sizeof(S2C_Pong) == 8, "S2C_Pong size must be 8");
