// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Game/Protocol_Client/Protocol_InGame.h" // EWeaponType 정의 위치

#include "MainAnimInstance.generated.h"

class AMainCharacter;

/**
 * AManagerCharacter의 데이터를 매 프레임 캐싱하여
 * 애니메이션 블루프린트(ABP)에 형변환 오버헤드 없이 전달하는 뼈대 AnimInstance.
 *
 * 데이터 소스: 이동=무브먼트 컴포넌트 / 조준·사격=GAS 태그 / 무기=PlayerState / 사망=HP / 왼손IK=장착 무기 소켓.
 * ABP는 전부 읽기만(BlueprintReadOnly). 재장전(bIsReloading)·몽타주는 ABP/어빌리티에서 처리.
 */
UCLASS(Blueprintable, BlueprintType)
class MANAGER_API UMainAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMainAnimInstance();

	// 최초 1회 (BeginPlay 역할) — 캐릭터 캐싱
	virtual void NativeInitializeAnimation() override;

	// 매 프레임 (Tick 역할) — 제어 변수 갱신
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 캐릭터 참조 (최초 1회 캐싱하여 매 프레임 Cast 방지) */
	UPROPERTY(BlueprintReadOnly, Category = "Reference")
	AMainCharacter* OwningCharacter;

	//================ 이동(하체) ================
	/** 속도 풀 벡터 — 점프/추락 판정용 Z 포함 (To Falling 컨듀잇에서 Velocity.Z 분기) */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector Velocity;

	/** 왼손 IK 목표 위치 (World) — 장착 무기 그립에 손바닥 맞춤 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector LeftIKLocation;

	/** 수평 이동 속도 (블렌드스페이스 Y축, 보간됨) */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed;

	/** 이동 방향 −180~180 (블렌드스페이스 X축, 각도 보간됨) */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction;

	/** 이동 시작/정지 (속도 + 가속 입력 동시 충족) */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove;

	/** 공중 여부 (= IsFalling, 점프 상승 중에도 true) */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsInAir;

	/** 앉기 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsCrouched;

	//================ 전투(상체) ================
	/** 조준 여부 (GAS 태그 State.Movement.Aiming) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming;

	/** 사격 여부 (GAS 태그 State.Movement.Firing) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsFiring;

	/** 조준 좌우 각도 (Aim Offset Yaw) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float AO_Yaw;

	/** 조준 상하 각도 (Aim Offset Pitch) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float AO_Pitch;

	/** 현재 장착 무기 타입 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	EWeaponType CurrentWeaponType;

	/** 무기 스탠스 (ABP Blend Poses by int용) — 0 맨손 / 1 권총(HG) / 2 양손총 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 WeaponStance;

	//================ 전역 상태 ================
	/** 사망 여부 (HP 0) */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsDead;

private:
	/** OwningCharacter 유효성 보장(끊겼으면 재캐싱). 실패 시 false */
	bool EnsureOwner();

public:
	UFUNCTION(BlueprintCallable, Category = "Animation Montage")
	void PlayFireMontage(EWeaponType WeaponType);
	UFUNCTION(BlueprintCallable, Category = "Animation Montage")
	void PlayReloadMontage(EWeaponType WeaponType);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TMap<EWeaponType, UAnimMontage*> WeaponFireMontages;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TMap<EWeaponType, UAnimMontage*> WeaponReloadMontages;
};
