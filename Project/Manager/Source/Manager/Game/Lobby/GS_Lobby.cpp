// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/GS_Lobby.h"
AGS_Lobby::AGS_Lobby()
{
    LobbySlots.AddDefaulted(4);
    for (int32 i = 0; i < 4; ++i)
    {
        LobbySlots[i].SlotIndex = i;
    }
}

void AGS_Lobby::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGS_Lobby, LobbySlots); // 네트워크 복제 활성화
}

void AGS_Lobby::OnRep_LobbySlots()
{
    OnLobbyUpdated.Broadcast(); // 클라이언트에서 발판 액터들에게 "데이터 바뀜!" 알림
}