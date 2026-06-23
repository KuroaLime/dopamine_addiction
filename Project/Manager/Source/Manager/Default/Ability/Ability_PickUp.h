// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_PickUp.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_PickUp : public UPFGAbility
{
	GENERATED_BODY()
	
public:
	UAbility_PickUp();

public:
	virtual void LocalActivateWithOwner(AActor* InOwner) override;
	virtual void LocalCancelWithOwner(AActor* InOwner) override;

protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;
};
