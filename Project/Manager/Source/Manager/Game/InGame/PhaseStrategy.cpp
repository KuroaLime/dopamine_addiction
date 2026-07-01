// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/PhaseStrategy.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

void UPhaseStrategy::Initialize(AGameModeBase* InOwner)
{
    OwnerGameMode = InOwner;
}

UWorld* UPhaseStrategy::GetWorld() const
{
    if (OwnerGameMode)
        return OwnerGameMode->GetWorld();
    return nullptr;
}

IPhaseGameModeInterface* UPhaseStrategy::GetPhaseGameMode() const
{
    return Cast<IPhaseGameModeInterface>(OwnerGameMode);
}

IPhaseGameStateInterface* UPhaseStrategy::GetPhaseGameState() const
{
    if (UWorld* World = GetWorld())
        return Cast<IPhaseGameStateInterface>(World->GetGameState());
    return nullptr;
}

IPhasePlayerControllerInterface* UPhaseStrategy::GetPhasePlayerController() const
{
    if (UWorld* World = GetWorld())
        return Cast<IPhasePlayerControllerInterface>(World->GetFirstPlayerController());
    return nullptr;
}