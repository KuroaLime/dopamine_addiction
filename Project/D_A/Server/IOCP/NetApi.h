#pragma once
// NetApi.h
#include <cstdint>

struct ClientContext;

using NetSendPacketFn = void(*)(ClientContext*, uint16_t, const void*, uint16_t);
using NetDisconnectFn = void(*)(ClientContext*);

struct NetApi
{
    NetSendPacketFn SendPacket = nullptr;
    NetDisconnectFn Disconnect = nullptr;
};

extern NetApi g_net;

void NetApi_Bind(NetSendPacketFn sendFn, NetDisconnectFn disconnectFn);
