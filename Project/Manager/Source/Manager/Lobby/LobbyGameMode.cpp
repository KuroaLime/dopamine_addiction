// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include"Game/Lobby/GS_Lobby.h"
#include "LobbyController.h"
#include "ManagerGameState.h"

ALobbyGameMode::ALobbyGameMode() {
	PlayerControllerClass = ALobbyController::StaticClass();
	GameStateClass = AManagerGameState::StaticClass();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AGS_Lobby* GS = GetGameState<AGS_Lobby>();
    if (GS && NewPlayer->PlayerState)
    {
        for (FLobbySlotData& Slot : GS->LobbySlots)
        {
            if (Slot.PlayerState == nullptr)
            {
                Slot.PlayerState = NewPlayer->PlayerState;
                GS->OnLobbyUpdated.Broadcast();
                break;
            }
        }
    }
}

void ALobbyGameMode::Logout(AController* Exiting)
{
    AGS_Lobby* GS = GetGameState<AGS_Lobby>();
    if (GS && Exiting->PlayerState)
    {
        for (FLobbySlotData& Slot : GS->LobbySlots)
        {
            if (Slot.PlayerState == Exiting->PlayerState)
            {
                Slot.PlayerState = nullptr;
                Slot.bIsReady = false;
                GS->OnLobbyUpdated.Broadcast();
                break;
            }
        }
    }
    Super::Logout(Exiting);
}