// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"

void UTPSPhaseStrategy::OnPhaseStart()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TPS Phase Started"));
	}

	PhaseDuration = 3;

	LoadStage();
	StartPhaseTimer();
}

void UTPSPhaseStrategy::OnPhaseEnd()
{
	if(UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	UnloadStage();
}

void UTPSPhaseStrategy::OnTimerTick()
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

void UTPSPhaseStrategy::OnPlayerAction(AActor* Executor, FName ActionName)
{
	
}

void UTPSPhaseStrategy::LoadStage()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Loading TPS Level..."));
	}
}

void UTPSPhaseStrategy::UnloadStage()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Unloading TPS Level..."));
	}

	if (IPhasePlayerControllerInterface* PC = GetPhasePlayerController())
	{
		PC->SwitchMode(EGamePhase::Card);
		PC->SwitchToLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));
	}
}

void UTPSPhaseStrategy::StartPhaseTimer()
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
				&UTPSPhaseStrategy::OnTimerTick,
				1.0f,
				true
			);
		}
	}
}

void UTPSPhaseStrategy::OnPhaseTimeout()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("Ending Battle Royale..."));
	}

	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
		GM->ChangePhase(EGamePhase::Card);
}