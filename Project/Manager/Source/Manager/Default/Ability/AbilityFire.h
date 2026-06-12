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
	virtual bool TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload) override;

	// 레벨에 따른 능력치 계산
	float CalculateDamage(const int32& Base, const int32& Level);
	float CalculateRange(const int32& Base, const int32& Level);
};
