// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/PhaseStrategy.h"
#include "CardPhaseStrategy.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UCardPhaseStrategy : public UPhaseStrategy
{
	GENERATED_BODY()
	
public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual void OnTimerTick() override;

protected:
	virtual void LoadStage() override;
	virtual void UnloadStage() override;
	virtual void StartPhaseTimer() override;
	virtual void OnPhaseTimeout() override;
};
