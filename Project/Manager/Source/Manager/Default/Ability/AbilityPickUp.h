// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityPickUp.generated.h"

class ABaseItem;
/**
 *
 */
UCLASS(Blueprintable)
class MANAGER_API UAbilityPickUp : public UCustomAbility
{
	GENERATED_BODY()

public:
	UAbilityPickUp();

protected:
	virtual void ActivateAbility() override;

	void RealPickUp();

protected:
	// [설정] 에디터에서 지정할 몽타주 (줍는 모션)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* MontageToPlay;

	// [임시 저장] 애니메이션이 끝날 때까지 줍기로 한 아이템을 기억해둠
	UPROPERTY()
	ABaseItem* PendingItem;
};

