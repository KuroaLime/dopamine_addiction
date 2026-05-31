// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/PhaseStrategy.h"
#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"

AMainGameMode::AMainGameMode()
{
	CurrentPhase = EGamePhase::TPS;
	CurrentStrategy = nullptr;
}

void AMainGameMode::BeginPlay()
{
	Super::BeginPlay();

	ChangePhase(EGamePhase::TPS);
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void AMainGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

void AMainGameMode::ChangePhase(EGamePhase NewPhase)
{
	if (CurrentStrategy)
	{
		CurrentStrategy->OnPhaseEnd();
		CurrentStrategy = nullptr;
	}

	CurrentPhase = NewPhase;

	switch (NewPhase)
	{

	case EGamePhase::TPS:
	{
		UTPSPhaseStrategy* TPS = NewObject<UTPSPhaseStrategy>(this);
		TPS->Initialize(this);
		CurrentStrategy = TPS;
		break;
	}

	case EGamePhase::Card:
	{
		/*UCardPhaseStrategy* Card = NewObject<UCardPhaseStrategy>(this);
		Card->Initialize(this);
		CurrentStrategy = Card;*/
		break;
	}

	default:
		break;
	}

	if(CurrentStrategy)
	{
		CurrentStrategy->OnPhaseStart();
	}
}

void AMainGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
	if (CurrentStrategy)
	{
		CurrentStrategy->OnPlayerAction(Executor, ActionName);
	}
}