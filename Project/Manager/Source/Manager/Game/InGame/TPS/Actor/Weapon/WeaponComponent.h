// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "WeaponComponent.generated.h"

class UNiagaraSystem;
class UMainAnimInstance;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWeaponComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	UFUNCTION(NetMulticast, Reliable)
	virtual void Multicast_PlayFireFeedback(const FVector& MuzzleLocation, const FVector& TargetLocation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_FireSound;

	// 발사 시 총구→명중 방향으로 날아가는 총알 트레이서 (멀티캐스트에서 스폰, 모든 클라 표시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	UNiagaraSystem* BulletTracerFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Setting")
	EWeaponType WeaponType = EWeaponType::SMG;
public:

	UFUNCTION(NetMulticast,Reliable)
	virtual void Multicast_PlayReloadFeedback();

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon | Ammo")
	int32 CurrentAmmo=30;

	// 장전 중 잠금 — true인 동안 사격 불가. 서버에서 set, 복제됨.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon | Reload")
	bool bIsReloading = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Ammo")
	int32 MaxMagazineCapacity = 30;
public:
	UFUNCTION(BlueprintPure, Category = "Weapon | Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }


	UFUNCTION(BlueprintCallable, Category = "Weapon | Ammo")
	void ConsumeAmmo();
	UFUNCTION(BlueprintCallable, Category = "Weapon | Ammo")
	void SetCurrentAmmo(int32 NewAmmo);
public:
	virtual int32 GetMaxMagazineCapacity() const;

	// 장전 중인지 (사격 차단 판정용)
	UFUNCTION(BlueprintPure, Category = "Weapon | Reload")
	bool IsReloading() const { return bIsReloading; }

	// 서버: Duration초 동안 "장전 중"으로 잠금 (그 사이 사격 불가). 끝나면 자동 해제.
	void StartReloadLock(float Duration);

	// 연사 중 탄퍼짐(블룸) 각도(도, 실제 SpreadAngle에 더해지는 양). 복제 안 됨 — 로컬 클라이언트가
	// 자기 자신의 조준점 UI 표시용으로만 씀. 무기마다 MaxBloomAngle이 다르므로 값 자체로 무기별 차이가 자연히 반영됨.
	UFUNCTION(BlueprintPure, Category = "Weapon | Bloom")
	float GetCurrentBloomDegrees() const { return CurrentBloomDegrees; }
	void SetCurrentBloomDegrees(float NewDegrees) { CurrentBloomDegrees = NewDegrees; }

	// 클라이언트 발사 예측(Ability_Fire::LocalActivateWithOwner/Client_ExecuteFire) 전용 상태.
	// 이 무기 액터(캐릭터별로 따로 존재)에 저장해야 한다 — UPFGAbility는 클라이언트에서
	// AbilityInstance가 복제되지 않아(PFGASC.h의 NotReplicated) CDO(클래스 전체에 하나뿐인 객체)로
	// LocalActivateWithOwner를 호출하므로, 여기 대신 어빌리티 멤버에 저장하면 모든 캐릭터/세션이
	// 상태를 공유해버려 발사 도중 꼬인다.
	bool IsClientFiring() const { return bIsClientFiring; }
	void SetClientFiring(bool bFiring) { bIsClientFiring = bFiring; }
	int32 GetClientShotsFiredInBurst() const { return ClientShotsFiredInBurst; }
	void SetClientShotsFiredInBurst(int32 NewCount) { ClientShotsFiredInBurst = NewCount; }
	float GetLastClientFireTime() const { return LastClientFireTime; }
	void SetLastClientFireTime(float NewTime) { LastClientFireTime = NewTime; }
	FTimerHandle& GetClientFireTimerHandle() { return ClientFireTimerHandle; }
	FTimerHandle& GetClientFireRetryTimerHandle() { return ClientFireRetryTimerHandle; }

private:
	float CurrentBloomDegrees = 0.f;

	bool bIsClientFiring = false;
	int32 ClientShotsFiredInBurst = 0;
	float LastClientFireTime = 0.f;
	FTimerHandle ClientFireTimerHandle;
	FTimerHandle ClientFireRetryTimerHandle;

	// 소유 폰의 MainAnimInstance를 찾고, 컴포넌트의 WeaponType을 PlayerState 값과 동기화한다.
	// (Fire/Reload 피드백 멀티캐스트가 공유하는 보일러플레이트)
	UMainAnimInstance* ResolveOwnerAnimAndSyncWeapon();

	void ClearReloadLock();
	FTimerHandle ReloadLockTimerHandle;
};
