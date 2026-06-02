// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/InGame/Interface/PhaseGameModeInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "PhaseStrategy.generated.h"

class AGameModeBase;

/**
 * 
 */
UCLASS(Abstract)
class MANAGER_API UPhaseStrategy : public UObject
{
	GENERATED_BODY()
	
public:
	void Initialize(AGameModeBase* InOwner);

	virtual void OnPhaseStart() {}
	virtual void OnPhaseEnd() {}

	virtual void OnTimerTick() {}
	virtual void OnPlayerAction(AActor* Executor, FName ActionName) {}
	virtual UWorld* GetWorld() const override;

protected:
	UPROPERTY()
	AGameModeBase* OwnerGameMode = nullptr;

	int32 PhaseDuration = 0;
	FTimerHandle RoundTimerHandle;

protected:
	IPhaseGameModeInterface* GetPhaseGameMode() const;
	IPhaseGameStateInterface* GetPhaseGameState() const;
	IPhasePlayerControllerInterface* GetPhasePlayerController() const;

	virtual void LoadStage() {}
	virtual void UnloadStage() {}
	virtual void StartPhaseTimer() {}
	virtual void OnPhaseTimeout() {}
};
