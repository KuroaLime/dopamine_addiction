// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/InGame/Interface/PhaseGameModeInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "PhaseStrategy.generated.h"

class AGameModeBase;
class AMainGameMode;

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

	// GameMode를 Context로 사용: 페이즈 셋업 로직이 GameMode의 헬퍼/상태에 접근할 때 사용한다.
	AMainGameMode* GetMainGameMode() const;

	virtual void LoadStage() {}
	virtual void UnloadStage() {}
	virtual void StartPhaseTimer() {}
	virtual void OnPhaseTimeout() {}
};
