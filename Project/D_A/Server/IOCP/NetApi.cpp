// NetApi.cpp
#include "NetApi.h"

NetApi g_net;

void NetApi_Bind(NetSendPacketFn sendFn, NetDisconnectFn disconnectFn)
{
    g_net.SendPacket = sendFn;
    g_net.Disconnect = disconnectFn;
}
