// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_Death.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_Death : public UPFGAbility
{
	GENERATED_BODY()
	
public:
	UAbility_Death();

public:
	virtual void LocalActivateWithOwner(AActor* InOwner) override;
	virtual void LocalCancelWithOwner(AActor* InOwner) override;

protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

private:
	int32 RespawnTime;

	FTimerHandle ServerRespawnTimerHandle;

	void Server_ExecuteCountDown();
	void DropDeathGold();
};
