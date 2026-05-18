#pragma once
#include <cstdint>
#include <vector>
#include "Protocol.h"

struct ClientContext;

// ===========================================================================
// Type Definitions
// ===========================================================================

// ServerMain.cpp의 SendPacket과 시그니처가 일치하는 함수 포인터 타입
typedef void (*SendPacketFn)(ClientContext* c, uint16_t type, const void* payload, uint16_t payloadLen);


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
    // [Auth] 로그인/회원가입
    // -----------------------------------------------------------------------
    void SendLoginRes(ClientContext* c, LoginResult result);
    void SendRegisterRes(ClientContext* c, LoginResult result);


    // -----------------------------------------------------------------------
    // [Lobby] 방 목록 및 생성
    // -----------------------------------------------------------------------
    void SendRoomListRes(ClientContext* c, const std::vector<RoomInfoView>& rooms);
    void SendRoomCreateRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull);


    // -----------------------------------------------------------------------
    // [Room] 방 입장 및 퇴장
    // -----------------------------------------------------------------------
    void SendRoomJoinRes(ClientContext* c, RoomResult result, const RoomInfoView* roomOrNull);
    void SendRoomLeaveRes(ClientContext* c, RoomResult result);

    // -----------------------------------------------------------------------
    // [Room Action] 방장/레디 시스템
    // -----------------------------------------------------------------------
    void SendRoomReadyBrd(ClientContext* c, uint32_t sessionId, bool isReady);
    void SendRoomStartRes(ClientContext* c, RoomResult result);

    // -----------------------------------------------------------------------
    // 게임 시작 (Dedicated Server 이동 명령)
    // -----------------------------------------------------------------------
    void SendGameStart(ClientContext* c, const char* ip, uint16_t port, uint32_t ticket);


private:
    // 실제 전송을 담당하는 함수 포인터
    SendPacketFn m_send;

    // -----------------------------------------------------------------------
    // 데이터 직렬화 도구
    // -----------------------------------------------------------------------
    static void AppendU8(std::vector<char>& out, uint8_t v);
    static void AppendU16(std::vector<char>& out, uint16_t vNet);
    static void AppendU32(std::vector<char>& out, uint32_t vNet);
};