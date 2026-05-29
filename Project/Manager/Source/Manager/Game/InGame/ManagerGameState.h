// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Game/InGame/Card/Data/SeotdaTypes.h" 
#include "Net/UnrealNetwork.h"
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ManagerGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeUpdatedDelegate, int32, NewTime);
/**
 * 
 */
UCLASS()
class MANAGER_API AManagerGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Seotda")
    FSeotdaTable Table;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTimeUpdatedDelegate OnTimeUpdated;

	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime)
	int32 RemainingTime;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	UFUNCTION()
	void OnRep_RemainingTime();
};
