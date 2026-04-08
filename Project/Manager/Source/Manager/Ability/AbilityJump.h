// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/CustomAbility.h"
#include "AbilityJump.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbilityJump : public UCustomAbility
{
	GENERATED_BODY()

public:
	UAbilityJump();

protected:
	virtual void ActivateAbility() override;
};
