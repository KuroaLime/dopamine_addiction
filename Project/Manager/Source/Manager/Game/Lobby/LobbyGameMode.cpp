// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/LobbyGameMode.h"
#include "Game/Lobby/LobbyController.h"

ALobbyGameMode::ALobbyGameMode() {
	PlayerControllerClass = ALobbyController::StaticClass();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
}

void ALobbyGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
}
