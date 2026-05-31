// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/PhaseStrategy.h"
#include "TPSPhaseStrategy.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UTPSPhaseStrategy : public UPhaseStrategy
{
	GENERATED_BODY()
	
public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual void OnTimerTick() override;
	virtual void OnPlayerAction(AActor* Executor, FName ActionName) override;

protected:
	const int32 BattleRoyaleDuration = 10;
	int32 RemainingTime;
	FTimerHandle RoundTimerHandle;

	void StartBattleRoyale();
	void EndBattleRoyale();

	void LoadTPSLevel();
	void UnloadTPSLevel();
};
