// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"
#include "GameFramework/GameStateBase.h"

void UTPSPhaseStrategy::OnPhaseStart()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TPS Phase Started"));
	}

	LoadTPSLevel();

	StartBattleRoyale();
}

void UTPSPhaseStrategy::OnPhaseEnd()
{
	if(UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	UnloadTPSLevel();
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

		EndBattleRoyale();
	}
}

void UTPSPhaseStrategy::OnPlayerAction(AActor* Executor, FName ActionName)
{
	
}

void UTPSPhaseStrategy::StartBattleRoyale()
{
	if (IPhaseGameStateInterface* GS = GetPhaseGameState())
	{
		GS->SetRemainingTime(BattleRoyaleDuration);
		GS->BroadcastTimeUpdated(BattleRoyaleDuration);
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

void UTPSPhaseStrategy::EndBattleRoyale()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("Ending Battle Royale..."));
	}

	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
		GM->ChangePhase(EGamePhase::Card);
}

void UTPSPhaseStrategy::LoadTPSLevel()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Loading TPS Level..."));
	}
}

void UTPSPhaseStrategy::UnloadTPSLevel()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Unloading TPS Level..."));
	}

	if(IPhasePlayerControllerInterface* PC = GetPhasePlayerController())
	{
		PC->SwitchToLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));
	}
}