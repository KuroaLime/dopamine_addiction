// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_Crouch.generated.h"

/**
 * 앉기 어빌리티(홀드형). 입력 Started에서 활성, Completed에서 취소되어 Aim과 동일한 수명주기를 가진다.
 * 캐릭터 행동을 GAS로 일원화하기 위해 기존 직접 Crouch()/UnCrouch() 호출을 대체한다.
 */
UCLASS()
class MANAGER_API UAbility_Crouch : public UPFGAbility
{
	GENERATED_BODY()

public:
	UAbility_Crouch();

public:
	virtual void LocalActivateWithOwner(AActor* InOwner) override;
	virtual void LocalCancelWithOwner(AActor* InOwner) override;

protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

private:
	void ApplyCrouch(AActor* InOwner, bool bWantCrouch);
};
