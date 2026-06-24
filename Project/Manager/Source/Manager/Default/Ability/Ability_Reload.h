// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_Reload.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_Reload : public UPFGAbility
{
	GENERATED_BODY()
public:
	UAbility_Reload();


protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

};
