// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/PhaseStrategy.h"
#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"
#include "Game/InGame/Card/CardPhaseStrategy.h"

AMainGameMode::AMainGameMode()
{
	CurrentStrategy = nullptr;
}

void AMainGameMode::BeginPlay()
{
	Super::BeginPlay();

	UTPSPhaseStrategy* TPSStrategy = NewObject<UTPSPhaseStrategy>(this);
	TPSStrategy->Initialize(this);
	StrategyMap.Add(EGamePhase::TPS, TPSStrategy);

	UCardPhaseStrategy* CardStrategy = NewObject<UCardPhaseStrategy>(this);
	CardStrategy->Initialize(this);
	StrategyMap.Add(EGamePhase::Card, CardStrategy);

	BeginPhase(EGamePhase::TPS);
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void AMainGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

void AMainGameMode::BeginPhase(EGamePhase CurrPhase)
{
	if (StrategyMap.Contains(CurrPhase))
	{
		CurrentStrategy = StrategyMap[CurrPhase];
	}

	if (CurrentStrategy)
	{
		CurrentStrategy->OnPhaseStart();
	}
}

void AMainGameMode::EndPhase()
{
	if (CurrentStrategy)
	{
		CurrentStrategy->OnPhaseEnd();
	}
}

void AMainGameMode::ChangePhase(EGamePhase NewPhase)
{
	EndPhase();
	BeginPhase(NewPhase);
}

void AMainGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
	if (CurrentStrategy)
	{
		CurrentStrategy->OnPlayerAction(Executor, ActionName);
	}
}