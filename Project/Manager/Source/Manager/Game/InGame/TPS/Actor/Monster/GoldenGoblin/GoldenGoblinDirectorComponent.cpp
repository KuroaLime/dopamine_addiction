// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinDirectorComponent.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UGoldenGoblinDirectorComponent::UGoldenGoblinDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UGoldenGoblinDirectorComponent* UGoldenGoblinDirectorComponent::GetActive(const UObject* WorldContextObject)
{
	AGameModeBase* GM = UGameplayStatics::GetGameMode(WorldContextObject);
	return GM ? GM->FindComponentByClass<UGoldenGoblinDirectorComponent>() : nullptr;
}

void UGoldenGoblinDirectorComponent::ActivateForBattleRoyale()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bActive = true;
	bSpawnedThisPhase = false;
	PhaseStartTimeSeconds = World->GetTimeSeconds();
	LastCombatEventTimeSeconds = PhaseStartTimeSeconds;

	World->GetTimerManager().SetTimer(
		CheckTimerHandle,
		this,
		&UGoldenGoblinDirectorComponent::TrySpawnGoblin,
		CheckIntervalSeconds,
		true);
}

void UGoldenGoblinDirectorComponent::Deactivate()
{
	bActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimerHandle);
	}
}

void UGoldenGoblinDirectorComponent::NotifyCombatEvent()
{
	if (UWorld* World = GetWorld())
	{
		LastCombatEventTimeSeconds = World->GetTimeSeconds();
	}
}

void UGoldenGoblinDirectorComponent::TrySpawnGoblin()
{
	if (!bActive || bSpawnedThisPhase || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GoblinClass)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - PhaseStartTimeSeconds < MinSpawnDelaySeconds)
	{
		return;
	}
	if (Now - LastCombatEventTimeSeconds < CombatLullSeconds)
	{
		// 아직 소강 상태가 아님 - 다음 체크 주기에 재시도.
		return;
	}

	TArray<FVector> PlayerLocations;
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				PlayerLocations.Add(Pawn->GetActorLocation());
			}
		}
	}
	if (PlayerLocations.Num() == 0)
	{
		return;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		return;
	}

	const FVector Origin = PlayerLocations[FMath::RandRange(0, PlayerLocations.Num() - 1)];
	const float MinDistanceSq = FMath::Square(MinDistanceFromPlayers);

	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
	{
		FNavLocation CandidateLocation;
		if (!NavSys->GetRandomReachablePointInRadius(Origin, SpawnSearchRadius, CandidateLocation))
		{
			continue;
		}

		bool bFarEnoughFromAllPlayers = true;
		for (const FVector& PlayerLoc : PlayerLocations)
		{
			if (FVector::DistSquared(CandidateLocation.Location, PlayerLoc) < MinDistanceSq)
			{
				bFarEnoughFromAllPlayers = false;
				break;
			}
		}

		if (!bFarEnoughFromAllPlayers)
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (World->SpawnActor<AGoldenGoblinCharacter>(GoblinClass, CandidateLocation.Location, FRotator::ZeroRotator, Params))
		{
			bSpawnedThisPhase = true;
			World->GetTimerManager().ClearTimer(CheckTimerHandle);
		}
		return;
	}
}
