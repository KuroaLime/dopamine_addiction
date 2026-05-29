// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/GS_Lobby.h"
#include "Game/Lobby/LobbyController.h"
#include "Game/Lobby/UI/LobbyWidget.h"

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
    DOREPLIFETIME(AGS_Lobby, CreatedRoomNames);
}

void AGS_Lobby::OnRep_LobbySlots()
{
    OnLobbyUpdated.Broadcast(); // 클라이언트에서 발판 액터들에게 "데이터 바뀜!" 알림
}

void AGS_Lobby::AddRoomName(const FString& NewRoomName) {
    if (HasAuthority()) {
        CreatedRoomNames.Add(NewRoomName);

        OnRep_CreatedRooms();
    }
}

void AGS_Lobby::OnRep_CreatedRooms() {
    if (!GetWorld()) return;

    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        ALobbyController* PC = Cast<ALobbyController>(Iterator->Get());
        if (PC && PC->IsLocalController())
        {
            if (UUserWidget* ListWidget = PC->WidgetInstances.FindRef(ELobbyState::RoomList))
            {
                if (ULobbyWidget* LobbyWgt = Cast<ULobbyWidget>(ListWidget))
                {
                    //LobbyWgt->RefreshRoomList();
                }
            }
            break;
        }
    }
}