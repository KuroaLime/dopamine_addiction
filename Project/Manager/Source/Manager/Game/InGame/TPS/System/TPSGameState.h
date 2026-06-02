// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Net/UnrealNetwork.h"
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "TPSGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeUpdatedDelegate_TPS, int32, NewTime);
/**
 * 
 */
UCLASS()
class MANAGER_API ATPSGameState : public AGameStateBase,
								  public IPhaseGameStateInterface
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTimeUpdatedDelegate_TPS OnTimeUpdated;

	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime)
	int32 RemainingTime;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public:
	virtual int32 GetRemainingTime() const override { return RemainingTime; }
	virtual void SetRemainingTime(int32 NewTime) override { RemainingTime = NewTime; }
	virtual void BroadcastTimeUpdated(int32 NewTime) override
	{
		// 기존 델리게이트 재활용
		if (OnTimeUpdated.IsBound())
			OnTimeUpdated.Broadcast(NewTime);
	}

protected:
	UFUNCTION()
	void OnRep_RemainingTime();
};
