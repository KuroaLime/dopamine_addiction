// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityFire.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbilityFire : public UCustomAbility
{
	GENERATED_BODY()
public:
	UAbilityFire();

protected:
	virtual void ActivateAbility() override;
};
