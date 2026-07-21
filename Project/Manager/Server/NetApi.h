#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>

#include "Protocol_D.h"

struct ClientContext;

// ===========================================================================
// Type Definitions
// ===========================================================================

// ServerMain.cpp의 SendPacket과 시그니처가 일치하는 함수 포인터 타입
typedef void (*SendPacketFn)(
    ClientContext* c,
    uint16_t type,
    const void* payload,
    std::size_t payloadLen
    );


// ===========================================================================
// NetApi Class
// ===========================================================================

class NetApi
{
public:
    // 생성자: 패킷 전송 함수 포인터를 주입받음
    explicit NetApi(SendPacketFn fn) : m_send(fn) {}


    // -----------------------------------------------------------------------
    // [System] 연결 및 상태 관리
    // -----------------------------------------------------------------------
    void SendWelcome(ClientContext* c, uint32_t sessionId);
    void SendPong(ClientContext* c);


    // -----------------------------------------------------------------------
    // [Auth] 로그인 / 회원가입
    // -----------------------------------------------------------------------
    void SendLoginRes(ClientContext* c, LoginResult result);
    void SendRegisterRes(ClientContext* c, LoginResult result);


    // -----------------------------------------------------------------------
    // [Lobby] 방 목록 및 생성
    // -----------------------------------------------------------------------
    void SendRoomListRes(ClientContext* c, const std::vector<RoomInfoView>& rooms);
    void SendRoomCreateRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull);


    // -----------------------------------------------------------------------
    // [Room] 방 입장 / 퇴장 / 방 멤버 목록
    // -----------------------------------------------------------------------
    void SendRoomJoinRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull);
    void SendRoomLeaveRes(ClientContext* c, RoomResult result);

    // 방 안에 있는 플레이어 목록 전송
    // Payload:
    //   u32 roomId
    //   u8 memberCount
    //   반복:
    //     u32 sessionId
    //     u8 isHost
    //     u8 isReady
    //     u8 nicknameLen
    //     char nickname[nicknameLen]
    void SendRoomMemberList(
        ClientContext* c,
        uint32_t roomId,
        const std::vector<RoomMemberInfoView>& members
    );


    // -----------------------------------------------------------------------
    // [Room Action] 방장 / 레디 / 시작
    // -----------------------------------------------------------------------
    void SendRoomReadyBrd(ClientContext* c, uint32_t sessionId, bool isReady);
    void SendRoomStartRes(ClientContext* c, RoomResult result);


    // -----------------------------------------------------------------------
    // [Game] Dedicated Server 이동 명령
    // -----------------------------------------------------------------------
    void SendGameStart(ClientContext* c, const char* ip, uint16_t port, uint32_t ticket);
    void SendDediControlAck(
        ClientContext* c,
        PacketType ackType,
        uint32_t roomId,
        uint16_t port,
        uint32_t generation,
        uint64_t controlToken,
        bool accepted);


private:
    // 실제 전송을 담당하는 함수 포인터
    SendPacketFn m_send;


    // -----------------------------------------------------------------------
    // 직렬화 도구
    // -----------------------------------------------------------------------
    static void AppendU8(std::vector<char>& out, uint8_t v);
    static void AppendU16(std::vector<char>& out, uint16_t vNet);
    static void AppendU32(std::vector<char>& out, uint32_t vNet);
    static void AppendU64BE(std::vector<char>& out, uint64_t vHost);
};