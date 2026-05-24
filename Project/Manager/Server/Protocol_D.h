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
    S2C_ROOM_MEMBER_LIST = 122,

    // [Room] 방 퇴장
    C2S_ROOM_LEAVE_REQ = 130,
    S2C_ROOM_LEAVE_RES = 131,

    // [Room Action] 방장/레디 시스템
    C2S_ROOM_READY_REQ = 140, // 클라 -> 서버: "나 레디할게/취소할게"
    S2C_ROOM_READY_BRD = 141, // 서버 -> 클라: "누가 레디했대/취소했대" (Broadcast)
    C2S_ROOM_START_REQ = 150, // 방장 -> 서버: "게임 시작하자!"
    S2C_ROOM_START_RES = 151, // 서버 -> 클라: "다 레디 안 함" 또는 "시작 성공"

    // [In-Game] 게임 시작 및 서버 이동 (Session Handover)
    S2C_GAME_START = 200,
};

struct PacketHeader
{
    uint16_t size; // 전체 패킷 길이 (Header + Payload)
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
    OK = 0, // 첫 로그인 성공 (로비로 이동)
    OK_RECONNECT = 1, // 재접속 성공 (바로 인게임 진입)

    ID_NOT_FOUND = 10,
    WRONG_PASSWORD = 11,
    ALREADY_LOGGED_IN = 12,

    ID_ALREADY_EXISTS = 20, // 회원가입 시
    INVALID_FORMAT = 21,
};

// 게임 및 방 설정 상수
static constexpr uint8_t  ROOM_MAX_PLAYERS = 4;
static constexpr uint8_t  ROOM_TITLE_MAX = 32;     // UTF-8 바이트 기준
static constexpr uint16_t PACKET_SIZE_MAX = 4096;   // 최대 패킷 크기

// 방 상태 (표시용)
enum class RoomState : uint8_t
{
    WAITING = 0, // 대기 중
    IN_GAME = 1, // 게임 진행 중
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

    NOT_HOST = 30,          // 방장이 아닌데 시작 누름
    NOT_ALL_READY = 31,     // 참여자 중 레디 안 한 사람이 있음
    NEED_MORE_PLAYERS = 32, // 혼자 있는데 시작 누름 (최소 2명 필요)
};


// ===========================================================================
// Data Structures
// ===========================================================================

// 방 정보 구조체 (서버 내부 목록 관리 및 클라이언트 전송용)
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

struct RoomMemberInfoView
{
    uint32_t sessionId = 0;

    uint8_t isHost = 0;
    uint8_t isReady = 0;

    uint8_t nicknameLen = 0;
    char    nickname[MAX_NICKNAME_LEN]{};
};


// ===========================================================================
// 참고용 주석
// ===========================================================================
/*
    [S2C_WELCOME]
      - u32 sessionId

    [C2S_LOGIN_REQ] / [C2S_REGISTER_REQ]
      - u8 idLen
      - char id[idLen]
      - u8 pwLen
      - char pw[pwLen]

    [S2C_LOGIN_RES] / [S2C_REGISTER_RES]
      - u8 result (LoginResult)

    [C2S_ROOM_LIST_REQ]
      - (Empty)

    [S2C_ROOM_LIST_RES]
      - u16 roomCount
      - List<RoomInfoView>

    [C2S_ROOM_CREATE_REQ]
      - u8 titleLen
      - char title[titleLen]

    [S2C_ROOM_CREATE_RES]
      - u8 result (RoomResult)
      - (If OK) RoomInfoView

    [C2S_ROOM_JOIN_REQ]
      - u32 roomId

    [S2C_ROOM_JOIN_RES]
      - u8 result
      - (If OK) RoomInfoView

    [S2C_GAME_START]
      - u8 ipLen
      - char ip[ipLen]
      - u32 ticket
      - u16 port

    [C2S_REGISTER_REQ]
      - u8 idLen
      - char id[idLen]
      - u8 pwLen
      - char pw[pwLen]
      - u8 nicknameLen              // optional, 없으면 id를 nickname으로 사용
      - char nickname[nicknameLen]

    [C2S_LOGIN_REQ]
      - u8 idLen
      - char id[idLen]
      - u8 pwLen
      - char pw[pwLen]

    [S2C_ROOM_MEMBER_LIST]
      - u32 roomId
      - u8 memberCount
      - 반복:
        - u32 sessionId
        - u8 isHost
        - u8 isReady
        - u8 nicknameLen
        - char nickname[nicknameLen]
*/