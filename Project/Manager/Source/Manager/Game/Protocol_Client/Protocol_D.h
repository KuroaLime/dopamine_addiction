// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Protocol_D.generated.h"
/**
 * 
 */

 // ===========================================================================
 // Packet Types & Header
 // ===========================================================================


USTRUCT(BlueprintType)
struct FRoomMemberInfoView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 sessionId = 0;

    UPROPERTY(BlueprintReadWrite)
    bool isHost = false;

    UPROPERTY(BlueprintReadWrite)
    bool isReady = false;

    UPROPERTY(BlueprintReadWrite)
    FString nickname;
};

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

    FString   title;
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
*/

namespace
{
    void AppendU8(std::vector<char>& out, uint8_t v)
    {
        out.push_back(static_cast<char>(v));
    }

    void AppendU16(std::vector<char>& out, uint16_t vNet)
    {
        char b[2];
        std::memcpy(b, &vNet, 2);
        out.insert(out.end(), b, b + 2);
    }

    void AppendU32(std::vector<char>& out, uint32_t vNet)
    {
        char b[4];
        std::memcpy(b, &vNet, 4);
        out.insert(out.end(), b, b + 4);
    }

    bool ReadU8(const char* data, size_t len, size_t& off, uint8_t& out)
    {
        if (off + 1 > len) return false;
        out = static_cast<uint8_t>(data[off]);
        off += 1;
        return true;
    }

    bool ReadU16(const char* data, size_t len, size_t& off, uint16_t& outHost)
    {
        if (off + 2 > len) return false;
        uint16_t net = 0;
        std::memcpy(&net, data + off, 2);
        outHost = ntohs(net);
        off += 2;
        return true;
    }

    bool ReadU32(const char* data, size_t len, size_t& off, uint32_t& outHost)
    {
        if (off + 4 > len) return false;
        uint32_t net = 0;
        std::memcpy(&net, data + off, 4);
        outHost = ntohl(net);
        off += 4;
        return true;
    }

    std::string ReadString(const char* data, size_t len, size_t& off, size_t strLen)
    {
        if (off + strLen > len) return {};
        std::string s(data + off, data + off + strLen);
        off += strLen;
        return s;
    }

    const char* LoginResultToString(LoginResult r)
    {
        switch (r)
        {
        case LoginResult::OK:                return "OK";
        case LoginResult::OK_RECONNECT:      return "OK_RECONNECT";
        case LoginResult::ID_NOT_FOUND:      return "ID_NOT_FOUND";
        case LoginResult::WRONG_PASSWORD:    return "WRONG_PASSWORD";
        case LoginResult::ALREADY_LOGGED_IN: return "ALREADY_LOGGED_IN";
        case LoginResult::ID_ALREADY_EXISTS: return "ID_ALREADY_EXISTS";
        case LoginResult::INVALID_FORMAT:    return "INVALID_FORMAT";
        default:                             return "UNKNOWN_LOGIN_RESULT";
        }
    }

    const FString* LoginResultToFString(LoginResult r)
    {
        switch (r)
        {
        case LoginResult::OK:                return new FString(TEXT("OK"));
        case LoginResult::OK_RECONNECT:      return new FString(TEXT("OK_RECONNECT"));
        case LoginResult::ID_NOT_FOUND:      return new FString(TEXT("ID_NOT_FOUND"));
        case LoginResult::WRONG_PASSWORD:    return new FString(TEXT("WRONG_PASSWORD"));
        case LoginResult::ALREADY_LOGGED_IN: return new FString(TEXT("ALREADY_LOGGED_IN"));
        case LoginResult::ID_ALREADY_EXISTS: return new FString(TEXT("ID_ALREADY_EXISTS"));
        case LoginResult::INVALID_FORMAT:    return new FString(TEXT("INVALID_FORMAT"));
        default:                             return new FString(TEXT("UNKNOWN_LOGIN_RESULT"));
        }
	}

    const char* RoomResultToString(RoomResult r)
    {
        switch (r)
        {
        case RoomResult::OK:                return "OK";
        case RoomResult::INVALID_ROOM:      return "INVALID_ROOM";
        case RoomResult::FULL:              return "FULL";
        case RoomResult::IN_GAME:           return "IN_GAME";
        case RoomResult::ALREADY_IN_ROOM:   return "ALREADY_IN_ROOM";
        case RoomResult::NOT_IN_ROOM:       return "NOT_IN_ROOM";
        case RoomResult::BAD_PAYLOAD:       return "BAD_PAYLOAD";
        case RoomResult::TITLE_TOO_LONG:    return "TITLE_TOO_LONG";
        case RoomResult::NOT_HOST:          return "NOT_HOST";
        case RoomResult::NOT_ALL_READY:     return "NOT_ALL_READY";
        case RoomResult::NEED_MORE_PLAYERS: return "NEED_MORE_PLAYERS";
        default:                            return "UNKNOWN_ROOM_RESULT";
        }
    }

    const char* RoomStateToString(RoomState s)
    {
        switch (s)
        {
        case RoomState::WAITING: return "WAITING";
        case RoomState::IN_GAME: return "IN_GAME";
        default:                 return "UNKNOWN_ROOM_STATE";
        }
    }

    struct RoomViewParsed
    {
        uint32_t roomId = 0;
        RoomState state = RoomState::WAITING;
        uint8_t curPlayers = 0;
        uint8_t maxPlayers = 0;
        uint32_t hostId = 0;
        std::string title;
    };

    bool ParseRoomView(const char* payload, size_t payloadLen, size_t& off, RoomViewParsed& out)
    {
        uint32_t roomId = 0;
        uint8_t state = 0;
        uint8_t curPlayers = 0;
        uint8_t maxPlayers = 0;
        uint32_t hostId = 0;
        uint8_t titleLen = 0;

        if (!ReadU32(payload, payloadLen, off, roomId)) return false;
        if (!ReadU8(payload, payloadLen, off, state)) return false;
        if (!ReadU8(payload, payloadLen, off, curPlayers)) return false;
        if (!ReadU8(payload, payloadLen, off, maxPlayers)) return false;
        if (!ReadU32(payload, payloadLen, off, hostId)) return false;
        if (!ReadU8(payload, payloadLen, off, titleLen)) return false;

        std::string title = ReadString(payload, payloadLen, off, titleLen);
        if (title.size() != titleLen) return false;

        out.roomId = roomId;
        out.state = static_cast<RoomState>(state);
        out.curPlayers = curPlayers;
        out.maxPlayers = maxPlayers;
        out.hostId = hostId;
        out.title = std::move(title);
        return true;
    }

    RoomInfoView ToRoomInfoView(RoomViewParsed& in)
    {
        RoomInfoView out{};

        out.roomId = in.roomId;
        out.state = in.state;
        out.curPlayers = in.curPlayers;
        out.maxPlayers = in.maxPlayers;
        out.hostId = in.hostId;
        out.title = FString(UTF8_TO_TCHAR(in.title.c_str()));

        return out;
    }

    bool ParseRoomMemberList(const char* payload, size_t payloadLen, size_t& off, uint32_t& outRoomId, TArray<FRoomMemberInfoView>& outMembers)
    {
        if (!ReadU32(payload, payloadLen, off, outRoomId)) return false;

        uint8_t memberCount = 0;
        if (!ReadU8(payload, payloadLen, off, memberCount)) return false;

        for (uint8_t i = 0; i < memberCount; ++i)
        {
            FRoomMemberInfoView member;
            uint32_t sId;
            uint8_t isHost, isReady, nickLen;

            if (!ReadU32(payload, payloadLen, off, sId)) return false;
            if (!ReadU8(payload, payloadLen, off, isHost)) return false;
            if (!ReadU8(payload, payloadLen, off, isReady)) return false;
            if (!ReadU8(payload, payloadLen, off, nickLen)) return false;

            std::string nickStr = ReadString(payload, payloadLen, off, nickLen);

            member.sessionId = static_cast<int32>(sId);
            member.isHost = (isHost == 1);
            member.isReady = (isReady == 1);
            member.nickname = FString(UTF8_TO_TCHAR(nickStr.c_str()));

            outMembers.Add(member);
        }
        return true;
    }
}
