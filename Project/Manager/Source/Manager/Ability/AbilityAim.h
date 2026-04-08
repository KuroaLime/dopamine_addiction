// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/CustomAbility.h"
#include "AbilityAim.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbilityAim : public UCustomAbility
{
	GENERATED_BODY()
public:
	UAbilityAim();

protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

protected:
	FTimerHandle ZoomTimerHandle;
};