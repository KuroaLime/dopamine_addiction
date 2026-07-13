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
	// 쿨다운 중에 들어온 클릭 1회를 큐잉하는 단발 타이머 (연사 루프용 타이머와 별개).
	FTimerHandle PendingServerShotTimerHandle;
	FTimerHandle PendingClientShotTimerHandle;
	// 반동을 여러 프레임에 나눠 적용(드레인)하는 타이머. 여러 발이 겹쳐도 유실 없이 PendingRecoil*에 누적된다.
	FTimerHandle RecoilStepTimerHandle;
	float PendingRecoilPitch = 0.f;
	float PendingRecoilYaw = 0.f;
	bool bIsServerFire;
	bool bIsClientFire;
	float LastClientFireTime;
	float LastServerFireTime;

	void Server_ExecuteFire();
	void Client_ExecuteFire(AActor* InOwner);
	void ApplyRecoilKick(ACharacter* Character, float TotalPitchDegrees, float TotalYawDegrees);
	float CalculateDamage(int32 Base, int32 Level) const;
	float CalculateRange(int32 Base, int32 Level) const;
	float CalculateFireRate(int32 Base, int32 Level) const;
};
