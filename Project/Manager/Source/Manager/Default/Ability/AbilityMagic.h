// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityMagic.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbilityMagic : public UCustomAbility
{
	GENERATED_BODY()
public:
	UAbilityMagic();

protected:
	virtual void ActivateAbility() override;
};
