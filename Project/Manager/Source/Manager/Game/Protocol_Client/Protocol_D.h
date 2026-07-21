// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#ifdef GetObject
#undef GetObject
#endif

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
    // [System] ���� Ȯ�� �� �ʱ�ȭ
    C2S_PING = 1,
    S2C_PONG = 2,
    S2C_WELCOME = 10,

    // [Auth] �α���/ȸ������
    C2S_LOGIN_REQ = 20,
    S2C_LOGIN_RES = 21,
    C2S_REGISTER_REQ = 22,
    S2C_REGISTER_RES = 23,

    // [Lobby] �� ��� ��ȸ
    C2S_ROOM_LIST_REQ = 100,
    S2C_ROOM_LIST_RES = 101,

    // [Room] �� ����
    C2S_ROOM_CREATE_REQ = 110,
    S2C_ROOM_CREATE_RES = 111,

    // [Room] �� ����
    C2S_ROOM_JOIN_REQ = 120,
    S2C_ROOM_JOIN_RES = 121,
    S2C_ROOM_MEMBER_LIST = 122,

    // [Room] �� ����
    C2S_ROOM_LEAVE_REQ = 130,
    S2C_ROOM_LEAVE_RES = 131,

    // [Room Action] ����/���� �ý���
    C2S_ROOM_READY_REQ = 140, // Ŭ�� -> ����: "�� �����Ұ�/����Ұ�"
    S2C_ROOM_READY_BRD = 141, // ���� -> Ŭ��: "���� �����ߴ�/����ߴ�" (Broadcast)
    C2S_ROOM_START_REQ = 150, // ���� -> ����: "���� ��������!"
    S2C_ROOM_START_RES = 151, // ���� -> Ŭ��: "�� ���� �� ��" �Ǵ� "���� ����"

    // [In-Game] ���� ���� �� ���� �̵� (Session Handover)
    S2C_GAME_START = 200,

    D2L_MATCH_END_NOTIFY = 300,
    D2L_SERVER_READY_NOTIFY = 301,
    L2D_MATCH_END_ACK = 302,
    L2D_SERVER_READY_ACK = 303,
};

struct PacketHeader
{
    uint16_t size; // ��ü ��Ŷ ���� (Header + Payload)
    uint16_t type; // PacketType
};
static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");


// ===========================================================================
// Constants & Enums
// ===========================================================================

static constexpr uint8_t MAX_ID_CHAR_LEN = 8;
static constexpr uint8_t MAX_ID_LEN = MAX_ID_CHAR_LEN * 4;
static constexpr uint8_t MAX_PW_LEN = 16;

enum class LoginResult : uint8_t
{
    OK = 0, // ù �α��� ���� (�κ�� �̵�)
    OK_RECONNECT = 1, // ������ ���� (�ٷ� �ΰ��� ����)

    ID_NOT_FOUND = 10,
    WRONG_PASSWORD = 11,
    ALREADY_LOGGED_IN = 12,

    ID_ALREADY_EXISTS = 20, // ȸ������ ��
    INVALID_FORMAT = 21,
};

// ���� �� �� ���� ���
static constexpr uint8_t  ROOM_MAX_PLAYERS = 4;
static constexpr uint8_t  ROOM_TITLE_MAX = 96;     // UTF-8 ����Ʈ ����
static constexpr uint16_t PACKET_SIZE_MAX = 4096;   // �ִ� ��Ŷ ũ��

// �� ���� (ǥ�ÿ�)
enum class RoomState : uint8_t
{
    WAITING = 0, // ��� ��
    IN_GAME = 1, // ���� ���� ��
};

// ��û ó�� ��� �ڵ�
enum class RoomResult : uint8_t
{
    OK = 0,

    // ������ ����
    INVALID_ROOM = 1,
    FULL = 2,
    IN_GAME = 3,

    // ���� ����
    ALREADY_IN_ROOM = 10,
    NOT_IN_ROOM = 11,

    // ������ ����
    BAD_PAYLOAD = 20,
    TITLE_TOO_LONG = 21,

    NOT_HOST = 30,          // ������ �ƴѵ� ���� ����
    NOT_ALL_READY = 31,     // ������ �� ���� �� �� ����� ����
    NEED_MORE_PLAYERS = 32, // ȥ�� �ִµ� ���� ���� (�ּ� 2�� �ʿ�)
};


// ===========================================================================
// Data Structures
// ===========================================================================

// �� ���� ����ü (���� ���� ��� ���� �� Ŭ���̾�Ʈ ���ۿ�)
struct RoomInfoView
{
    uint32_t  roomId = 0;
    RoomState state = RoomState::WAITING;
    uint8_t   curPlayers = 0;
    uint8_t   maxPlayers = ROOM_MAX_PLAYERS;

    uint32_t  hostId = 0; // ������ Session ID

    FString   title;
};


// ===========================================================================
// ������ �ּ�
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
        FUTF8ToTCHAR Converted(in.title.data(), static_cast<int32>(in.title.size()));
        out.title = FString(Converted.Length(), Converted.Get());

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
