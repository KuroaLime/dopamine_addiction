// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/CustomAbility.h"
#include "AbilityThrow.generated.h"

class AGrenadeItem;

/**
 * 
 */
UCLASS(Blueprintable)
class MANAGER_API UAbilityThrow : public UCustomAbility
{
	GENERATED_BODY()
public:
	UAbilityThrow();

	// [설정] 던지는 모션
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Config")
	UAnimMontage* ThrowMontage;

	// [설정] 던지는 타이밍 (애니메이션에서 손을 뻗는 시간)
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Config")
	float ThrowDelay = 0.3f;

	// [설정] 생성 위치 소켓 이름
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Config")
	FName SocketName = TEXT("GrenadeSocket");

protected:
	// 1. 실행 가능 여부 체크 (인벤토리 검사)
	virtual bool CanExecute() const override;

	// 2. 어빌리티 시작 (몽타주 재생 & 타이머)
	virtual void ActivateAbility() override;

	// 3. 실제 투척 (타이머 종료 후 실행)
	void ExecuteThrow();

private:
	FTimerHandle ThrowTimerHandle;

	// 던질 아이템 임시 저장용
	UPROPERTY()
	AGrenadeItem* CachedGrenadeItem;
};
