// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhaseCharacterInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"

void UTPSPhaseStrategy::OnPhaseStart()
{
	PhaseDuration = 10;

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
	if (!GetWorld() || !GetWorld()->GetAuthGameMode()) return;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Loading TPS Level..."));
	}

	EWeaponType RoundWeapon = EWeaponType::SMG;
	if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
	{
		GS->SetRoundWeapon(RoundWeapon);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState)
		{
			if (IPhasePlayerStateInterface* PS = Cast<IPhasePlayerStateInterface>(PC->PlayerState))
			{
				PS->SetWeaponID(RoundWeapon);
			}

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				if (IPhaseCharacterInterface* IC = Cast<IPhaseCharacterInterface>(PlayerPawn))
				{
					IC->EquipWeapon(RoundWeapon);
				}
			}
		}
	}
}

void UTPSPhaseStrategy::UnloadStage()
{
	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
	{
		GM->BroadcastSwitchMode(EGamePhase::Card);
		GM->BroadcastSwitchLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));
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
	if (IPhaseGameModeInterface* GM = GetPhaseGameMode())
		GM->ChangePhase(EGamePhase::Card);
}