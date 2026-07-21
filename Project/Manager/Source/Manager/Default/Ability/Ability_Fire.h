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
	// 쿨다운이 안 끝난 상태로 클릭했을 때, 버튼을 누르고 있는 동안만 짧은 간격으로 재검사하는 타이머.
	// 쿨다운이 끝나는 순간 발사하고, 손을 떼면(release) 즉시 멈춘다.
	FTimerHandle ServerFireRetryTimerHandle;
	// 연사 중 실제 탄 판정 원뿔(SpreadAngle)에 누적되는 블룸. 트리거를 놓으면(EndAbility) 0으로 리셋.
	float CurrentBloomAngle = 0.f;
	// 이번 트리거 홀드에서 지금까지 쏜 발 수. BloomStartShotCount발까지는 블룸이 늘지 않는다.
	int32 ShotsFiredInBurst = 0;
	bool bIsServerFire;
	float LastServerFireTime;
	// 클라이언트 발사 예측 상태(bIsClientFire, ClientShotsFiredInBurst, 타이머 등)는 여기 두지 않는다.
	// LocalActivateWithOwner/LocalCancelWithOwner는 클라이언트에서 CDO(클래스 전체 공유 객체)로 호출되므로,
	// 여기 멤버에 저장하면 모든 캐릭터/세션이 상태를 공유해 발사 도중 꼬인다. WeaponComponent에 저장한다.


	float ActiveServerFireRate;
	bool bServerFullAuto;
	void HandleServerFireLoop();
	void HandleServerFireRetry();

	void ClearServerFireTimers(UWorld* World);
	void Server_ExecuteFire();
	void Client_ExecuteFire(AActor* InOwner);
	float CalculateDamage(int32 Base, int32 Level) const;
	float CalculateRange(int32 Base, int32 Level) const;
	float CalculateFireRate(int32 Base, int32 Level) const;
	class UTpsPlayerMainHUD* ResolveHUD(AActor* InOwner) const;
};
