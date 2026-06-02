// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/CardPhaseStrategy.h"

void UCardPhaseStrategy::OnPhaseStart()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Card Phase Started"));
	}

	PhaseDuration = 3;

	LoadStage();
	StartPhaseTimer();
}

void UCardPhaseStrategy::OnPhaseEnd()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	UnloadStage();
}

void UCardPhaseStrategy::OnTimerTick()
{
	IPhaseGameStateInterface* GS = GetPhaseGameState();
	if (!GS) return;

	int32 NewTime = GS->GetRemainingTime() - 1;
	GS->SetRemainingTime(NewTime);
	GS->BroadcastTimeUpdated(NewTime);

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(1, 1.1f, FColor::Yellow,
			FString::Printf(TEXT("Remaining Time: %d"), NewTime));

	if (NewTime <= 0)
	{
		if (UWorld* World = GetWorld())
			World->GetTimerManager().ClearTimer(RoundTimerHandle);

		OnPhaseTimeout();
	}
}

void UCardPhaseStrategy::OnPlayerAction(AActor* Executor, FName ActionName)
{

}

void UCardPhaseStrategy::LoadStage()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Loading Card Level..."));
	}
}

void UCardPhaseStrategy::UnloadStage()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Unloading Card Level..."));
	}

	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
	{
		GM->BroadcastSwitchMode(EGamePhase::TPS);
		GM->BroadcastSwitchLevel(TEXT("Card_Game_Stage"), TEXT("TPS_Game_Stage"));
	}
}

void UCardPhaseStrategy::StartPhaseTimer()
{
	if (IPhaseGameStateInterface* GS = GetPhaseGameState())
	{
		GS->SetRemainingTime(PhaseDuration);
		GS->BroadcastTimeUpdated(PhaseDuration);
	}

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(RoundTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				RoundTimerHandle,
				this,
				&UCardPhaseStrategy::OnTimerTick,
				1.0f,
				true
			);
		}
	}
}

void UCardPhaseStrategy::OnPhaseTimeout()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("Ending Card..."));
	}

	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
		GM->ChangePhase(EGamePhase::TPS);
}