// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "ManagerGameMode.generated.h"

/**
 * Simple GameMode for a third person game
 */
class APlayerController;
class AController;

UCLASS(abstract)
class AManagerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AManagerGameMode();

public:
	virtual void BeginPlay() override;
	// 타이머 함수
	void RoundTimerTick();

private:
	FTimerHandle RoundTimerHandle;
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Manager")
	class USpawnManagerComponent* SpawnManager;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};