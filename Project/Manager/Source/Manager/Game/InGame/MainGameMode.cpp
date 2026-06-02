// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/PhaseStrategy.h"

AMainGameMode::AMainGameMode()
{
	CurrentStrategy = nullptr;
}

void AMainGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitStrategy();
	BeginPhase(InitialPhase);
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

void AMainGameMode::InitStrategy()
{
	for (auto& Pair : StrategyClassMap)
	{
		if (!Pair.Value) continue;

		UPhaseStrategy* Strategy = NewObject<UPhaseStrategy>(this, Pair.Value);
		if (Strategy)
		{
			Strategy->Initialize(this);
			StrategyMap.Add(Pair.Key, Strategy);
		}
	}
}