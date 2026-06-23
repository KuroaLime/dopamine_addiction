// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_Shop.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_Shop : public UPFGAbility
{
	GENERATED_BODY()
	
public:
	UAbility_Shop();

public:
	virtual void LocalActivateWithOwner(AActor* InOwner) override;

protected:
	virtual void ActivateAbility() override;
};
