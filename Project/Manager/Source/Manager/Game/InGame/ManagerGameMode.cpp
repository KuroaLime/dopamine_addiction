// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManagerGameMode.h"
#include "TimerManager.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "PlayerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/ManagerCharacter.h"

// GameState 병합 필요

AManagerGameMode::AManagerGameMode()
{
	SpawnManager = CreateDefaultSubobject<USpawnManagerComponent>(TEXT("SpawnManager"));
}

void AManagerGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Dedicated Server에서는 BeginPlay에서 게임을 자동 시작하지 않는다.
	// 클라이언트 접속 시 PostLogin에서 인원 수를 확인하고,
	// RequiredPlayerCount명 이상이 모이면 StartSeotdaGame()을 호출한다.
}

void AManagerGameMode::RoundTimerTick()
{

}

AActor* AManagerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (SpawnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("Initialize Random Spawn..."));

		if (SpawnManager->GetAvailableSpawnCount() == 0)
		{
			SpawnManager->InitializeSpawnPoints();
		}

		AA_Spawn* RandomSpawn = SpawnManager->GetUniqueRandomSpawnActor();

		if (RandomSpawn)
		{
			return RandomSpawn;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AManagerGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform OffsetTransform = SpawnTransform;
	FVector NewLocation = OffsetTransform.GetLocation() + FVector(0.0f, 0.0f, 100.0f);

	OffsetTransform.SetLocation(NewLocation);

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OffsetTransform);
}