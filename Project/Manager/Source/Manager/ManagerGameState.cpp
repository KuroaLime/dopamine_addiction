// Fill out your copyright notice in the Description page of Project Settings.


#include "ManagerGameState.h"
#include "Net/UnrealNetwork.h"
#include "Lobby/LobbyController.h"
#include "Lobby/LobbyWidget.h"

void AManagerGameState::OnRep_RemainingTime() {
	if (OnTimeUpdated.IsBound())
		OnTimeUpdated.Broadcast(RemainingTime);
}

void AManagerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AManagerGameState, Table);
	DOREPLIFETIME(AManagerGameState, RemainingTime);
	DOREPLIFETIME(AManagerGameState, CreatedRoomNames);
}

void AManagerGameState::AddRoomName(const FString& NewRoomName) {
    if (HasAuthority()) {
        CreatedRoomNames.Add(NewRoomName);

        OnRep_CreatedRooms();
    }
}

void AManagerGameState::OnRep_CreatedRooms() {
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
                    LobbyWgt->RefreshRoomList(CreatedRoomNames);
                }
            }
            break;
        }
    }
}