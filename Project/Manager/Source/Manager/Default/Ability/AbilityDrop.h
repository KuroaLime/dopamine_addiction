// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityDrop.generated.h"

class ABaseItem;
/**
 * 
 */
UCLASS(Blueprintable)
class MANAGER_API UAbilityDrop : public UCustomAbility
{
	GENERATED_BODY()

public:
	UAbilityDrop();

	// 인자 없는 함수 (부모 클래스에서 연결해줌)
	virtual void ActivateAbility() override;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* MontageToPlay;

	// [함수] 애니메이션(타이머)이 끝나면 진짜로 아이템을 버리는 함수
	void RealDrop();
};
