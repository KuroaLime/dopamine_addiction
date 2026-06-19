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
	virtual void LocalCancelWithOwner(AActor* InOwner) override;

protected:
	// 서버: 쿨다운/태그 처리 후 즉시 종료
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

private:
	FTimerHandle ServerFireTimerHandle;
	FTimerHandle ClientFireTimerHandle;
	bool bIsServerFire;
	bool bIsClientFire;

	void Server_ExecuteFire();
	void Client_ExecuteFire(AActor* InOwner);
	float CalculateDamage(int32 Base, int32 Level) const;
	float CalculateRange(int32 Base, int32 Level) const;
};
