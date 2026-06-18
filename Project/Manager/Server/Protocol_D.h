#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
    C2S_PING = 1,
    S2C_PONG = 2,
    S2C_WELCOME = 10,

    C2S_LOGIN_REQ = 20,
    S2C_LOGIN_RES = 21,
    C2S_REGISTER_REQ = 22,
    S2C_REGISTER_RES = 23,

    C2S_ROOM_LIST_REQ = 100,
    S2C_ROOM_LIST_RES = 101,

    C2S_ROOM_CREATE_REQ = 110,
    S2C_ROOM_CREATE_RES = 111,

    C2S_ROOM_JOIN_REQ = 120,
    S2C_ROOM_JOIN_RES = 121,
    S2C_ROOM_MEMBER_LIST = 122,

    C2S_ROOM_LEAVE_REQ = 130,
    S2C_ROOM_LEAVE_RES = 131,

    C2S_ROOM_READY_REQ = 140,
    S2C_ROOM_READY_BRD = 141,

    C2S_ROOM_START_REQ = 150,
    S2C_ROOM_START_RES = 151,

    S2C_GAME_START = 200,
};

struct PacketHeader
{
    uint16_t size;
    uint16_t type;
};
static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");

static constexpr uint8_t MAX_ID_LEN = 16;
static constexpr uint8_t MAX_PW_LEN = 16;
static constexpr uint8_t MAX_NICKNAME_LEN = 16;

static constexpr uint8_t ROOM_MAX_PLAYERS = 4;
static constexpr uint8_t ROOM_TITLE_MAX = 96;
static constexpr uint16_t PACKET_SIZE_MAX = 4096;

enum class LoginResult : uint8_t
{
    OK = 0,
    OK_RECONNECT = 1,

    ID_NOT_FOUND = 10,
    WRONG_PASSWORD = 11,
    ALREADY_LOGGED_IN = 12,

    ID_ALREADY_EXISTS = 20,
    INVALID_FORMAT = 21,
};

enum class RoomState : uint8_t
{
    WAITING = 0,
    IN_GAME = 1,
};

enum class RoomResult : uint8_t
{
    OK = 0,

    INVALID_ROOM = 1,
    FULL = 2,
    IN_GAME = 3,

    ALREADY_IN_ROOM = 10,
    NOT_IN_ROOM = 11,

    BAD_PAYLOAD = 20,
    TITLE_TOO_LONG = 21,

    NOT_HOST = 30,
    NOT_ALL_READY = 31,
    NEED_MORE_PLAYERS = 32,
};

struct RoomInfoView
{
    uint32_t roomId = 0;
    RoomState state = RoomState::WAITING;
    uint8_t curPlayers = 0;
    uint8_t maxPlayers = ROOM_MAX_PLAYERS;
    uint32_t hostId = 0;
    uint8_t titleLen = 0;
    char title[ROOM_TITLE_MAX]{};
};

struct RoomMemberInfoView
{
    uint32_t sessionId = 0;
    uint8_t isHost = 0;
    uint8_t isReady = 0;
    uint8_t nicknameLen = 0;
    char nickname[MAX_NICKNAME_LEN]{};
};
