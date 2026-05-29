#pragma once
#include <cstdint>

// ===========================================================================
// Packet Types & Header
// ===========================================================================

enum class PacketType : uint16_t
{
    // [System] 연결 확인 및 초기화
    C2S_PING = 1,
    S2C_PONG = 2,
    S2C_WELCOME = 10,

    // [Auth] 로그인/회원가입
    C2S_LOGIN_REQ = 20,
    S2C_LOGIN_RES = 21,
    C2S_REGISTER_REQ = 22,
    S2C_REGISTER_RES = 23,

    // [Lobby] 방 목록 조회
    C2S_ROOM_LIST_REQ = 100,
    S2C_ROOM_LIST_RES = 101,

    // [Room] 방 생성
    C2S_ROOM_CREATE_REQ = 110,
    S2C_ROOM_CREATE_RES = 111,

    // [Room] 방 입장
    C2S_ROOM_JOIN_REQ = 120,
    S2C_ROOM_JOIN_RES = 121,

    // [Room] 방 안 멤버 목록
    // 방 생성 / 방 입장 / 방 퇴장 / 레디 변경 시 서버가 방 인원들에게 전송
    S2C_ROOM_MEMBER_LIST = 122,

    // [Room] 방 퇴장
    C2S_ROOM_LEAVE_REQ = 130,
    S2C_ROOM_LEAVE_RES = 131,

    // [Room Action] 방장/레디 시스템
    C2S_ROOM_READY_REQ = 140, // 클라 -> 서버: 레디/레디 취소 요청
    S2C_ROOM_READY_BRD = 141, // 서버 -> 클라: 특정 플레이어 레디 상태 변경 브로드캐스트

    C2S_ROOM_START_REQ = 150, // 방장 -> 서버: 게임 시작 요청
    S2C_ROOM_START_RES = 151, // 서버 -> 방장: 게임 시작 결과

    // [In-Game] 게임 시작 및 서버 이동
    S2C_GAME_START = 200,
};

struct PacketHeader
{
    uint16_t size; // 전체 패킷 길이 Header + Payload
    uint16_t type; // PacketType
};
static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");


// ===========================================================================
// Constants & Enums
// ===========================================================================

static constexpr uint8_t MAX_ID_LEN = 16;
static constexpr uint8_t MAX_PW_LEN = 16;
static constexpr uint8_t MAX_NICKNAME_LEN = 16;

enum class LoginResult : uint8_t
{
    OK = 0,              // 첫 로그인 성공
    OK_RECONNECT = 1,    // 재접속 성공

    ID_NOT_FOUND = 10,
    WRONG_PASSWORD = 11,
    ALREADY_LOGGED_IN = 12,

    ID_ALREADY_EXISTS = 20,
    INVALID_FORMAT = 21,
};

// 게임 및 방 설정 상수
static constexpr uint8_t  ROOM_MAX_PLAYERS = 4;
static constexpr uint8_t  ROOM_TITLE_MAX = 32;      // UTF-8 바이트 기준
static constexpr uint16_t PACKET_SIZE_MAX = 4096;   // 최대 패킷 크기

// 방 상태
enum class RoomState : uint8_t
{
    WAITING = 0,
    IN_GAME = 1,
};

// 요청 처리 결과 코드
enum class RoomResult : uint8_t
{
    OK = 0,

    // 논리적 에러
    INVALID_ROOM = 1,
    FULL = 2,
    IN_GAME = 3,

    // 상태 에러
    ALREADY_IN_ROOM = 10,
    NOT_IN_ROOM = 11,

    // 데이터 에러
    BAD_PAYLOAD = 20,
    TITLE_TOO_LONG = 21,

    NOT_HOST = 30,
    NOT_ALL_READY = 31,
    NEED_MORE_PLAYERS = 32,
};


// ===========================================================================
// Data Structures
// ===========================================================================

// 방 정보 구조체
// 방 목록 / 방 생성 결과 / 방 입장 결과에서 사용
struct RoomInfoView
{
    uint32_t  roomId = 0;
    RoomState state = RoomState::WAITING;
    uint8_t   curPlayers = 0;
    uint8_t   maxPlayers = ROOM_MAX_PLAYERS;

    uint32_t  hostId = 0; // 방장의 Session ID

    uint8_t   titleLen = 0;
    char      title[ROOM_TITLE_MAX]{};
};

// 방 안 멤버 정보 구조체
// S2C_ROOM_MEMBER_LIST 패킷의 반복 요소로 사용
struct RoomMemberInfoView
{
    uint32_t sessionId = 0;

    uint8_t isHost = 0;
    uint8_t isReady = 0;

    uint8_t nicknameLen = 0;
    char    nickname[MAX_NICKNAME_LEN]{};
};


// ===========================================================================
// Packet Payload Reference
// ===========================================================================
/*
    [S2C_WELCOME]
      - u32 sessionId

    [C2S_LOGIN_REQ]
      - u8 idLen
      - char id[idLen]
      - u8 pwLen
      - char pw[pwLen]

    [C2S_REGISTER_REQ]
      - u8 idLen
      - char id[idLen]
      - u8 pwLen
      - char pw[pwLen]
      - u8 nicknameLen              // optional
      - char nickname[nicknameLen]  // optional

       nickname이 없는 구형 클라이언트도 허용 가능.
       서버에서 nickname이 없으면 id를 nickname으로 사용하면 됨.

    [S2C_LOGIN_RES] / [S2C_REGISTER_RES]
      - u8 result (LoginResult)

    [C2S_ROOM_LIST_REQ]
      - Empty

    [S2C_ROOM_LIST_RES]
      - u16 roomCount
      - 반복:
        - RoomInfoView

    [C2S_ROOM_CREATE_REQ]
      - u8 titleLen
      - char title[titleLen]

    [S2C_ROOM_CREATE_RES]
      - u8 result (RoomResult)
      - If OK:
        - RoomInfoView

    [C2S_ROOM_JOIN_REQ]
      - u32 roomId

    [S2C_ROOM_JOIN_RES]
      - u8 result (RoomResult)
      - If OK:
        - RoomInfoView

    [S2C_ROOM_MEMBER_LIST]
      - u32 roomId
      - u8 memberCount
      - 반복:
        - u32 sessionId
        - u8 isHost
        - u8 isReady
        - u8 nicknameLen
        - char nickname[nicknameLen]

    [C2S_ROOM_LEAVE_REQ]
      - Empty

    [S2C_ROOM_LEAVE_RES]
      - u8 result (RoomResult)

    [C2S_ROOM_READY_REQ]
      - u8 ready
        0 = ready 취소
        1 = ready

    [S2C_ROOM_READY_BRD]
      - u32 sessionId
      - u8 ready

    [C2S_ROOM_START_REQ]
      - Empty

    [S2C_ROOM_START_RES]
      - u8 result (RoomResult)

    [S2C_GAME_START]
      - u8 ipLen
      - char ip[ipLen]
      - u32 ticket
      - u16 port
*/