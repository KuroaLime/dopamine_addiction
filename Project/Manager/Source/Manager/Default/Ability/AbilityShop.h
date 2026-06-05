// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityShop.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbilityShop : public UCustomAbility
{
	GENERATED_BODY()
public:
	UAbilityShop();

protected:
	virtual void ActivateAbility() override;

};
