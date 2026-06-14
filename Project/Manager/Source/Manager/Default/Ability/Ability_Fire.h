// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_Fire.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_Fire : public UPFGAbility
{
	GENERATED_BODY()
	
public:
	UAbility_Fire();

public:
	// 클라이언트 로컬: 레이캐스트 + 발사 이펙트 + 히트 시 ServerRPC 전송
	virtual void LocalActivateWithOwner(AActor* InOwner) override;

	// 서버: 히트 이벤트 수신 후 실제 데미지 적용
	virtual bool TryActivateAbilityWithEvent(const FPFGGameplayEventData& Payload) override;

protected:
	// 서버: 쿨다운/태그 처리 후 즉시 종료
	virtual void ActivateAbility() override;

private:
	float CalculateDamage(int32 Base, int32 Level) const;
	float CalculateRange(int32 Base, int32 Level) const;
};
