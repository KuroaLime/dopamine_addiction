// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/InGame/PhaseGameModeInterface.h"
#include "MainGameMode.generated.h"

class UPhaseStrategy;

/**
 * 
 */
UCLASS()
class MANAGER_API AMainGameMode : public AGameModeBase,
								  public IPhaseGameModeInterface
{
	GENERATED_BODY()

public:
	AMainGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

public:
	virtual void BeginePhase(EGamePhase CurrPhase) override;
	virtual void EndPhase() override;
	virtual void ChangePhase(EGamePhase NewPhase) override;

public:
	void OnPlayerAction(AActor* Executor, FName ActionName);

protected:
	UPROPERTY()
	TMap<EGamePhase, TObjectPtr<UPhaseStrategy>> StrategyMap;

	UPROPERTY()
	UPhaseStrategy* CurrentStrategy;
};
