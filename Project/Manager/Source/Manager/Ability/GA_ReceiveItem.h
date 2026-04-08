// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/CustomAbility.h"
#include "GA_ReceiveItem.generated.h"

/**
 * 
 */
struct FCustomGameplayEventData;

UCLASS()
class MANAGER_API UGA_ReceiveItem : public UCustomAbility
{
	GENERATED_BODY()
protected:
	// 일반 ActivateAbility 대신 이벤트 데이터를 받는 버전을 오버라이드!
	virtual void ActivateAbilityWithEvent(const FCustomGameplayEventData& Payload) override;
};
