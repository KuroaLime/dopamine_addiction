// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainAnimInstance.h"
#include "Game/InGame/MainCharacter.h"
#include "Game/InGame/MainPlayerState.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "KismetAnimationLibrary.h"   // CalculateDirection
#include "GameplayTagContainer.h"

UMainAnimInstance::UMainAnimInstance()
{
	// 기본값 초기화
	Velocity = FVector::ZeroVector;
	LeftIKLocation = FVector::ZeroVector;
	LeftHandIKAlpha = 1.f;
	bIsReloading = false;
	GroundSpeed = 0.f;
	Direction = 0.f;
	bShouldMove = false;
	bIsInAir = false;
	bIsCrouched = false;

	bIsAiming = false;
	bIsFiring = false;
	AO_Yaw = 0.f;
	AO_Pitch = 0.f;
	CurrentWeaponType = EWeaponType::None;
	WeaponStance = 0;

	bIsDead = false;
}

void UMainAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 무거운 Cast를 피하려고 최초 1회만 캐싱
	OwningCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
}

bool UMainAnimInstance::EnsureOwner()
{
	if (OwningCharacter == nullptr)
	{
		OwningCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
	}
	return OwningCharacter != nullptr;
}

void UMainAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!EnsureOwner())
	{
		return;
	}

	// 1) 이동: 속도/방향 — 둘 다 부드럽게 보간(스냅 방지)
	Velocity = OwningCharacter->GetVelocity();
	const float TargetSpeed = Velocity.Size2D();
	const float TargetDirection = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningCharacter->GetActorRotation());

	const float SpeedInterpSpeed = 8.f;
	const float DirInterpSpeed = 8.f;

	GroundSpeed = FMath::FInterpTo(GroundSpeed, TargetSpeed, DeltaSeconds, SpeedInterpSpeed);
	// 방향은 각도(−180~180) → ±180 경계를 최단 경로로 (그냥 FInterpTo는 wraparound 깨짐)
	const float DeltaAngle = FMath::FindDeltaAngleDegrees(Direction, TargetDirection);
	Direction = FMath::UnwindDegrees(Direction + DeltaAngle * FMath::Clamp(DeltaSeconds * DirInterpSpeed, 0.f, 1.f));

	// 2) 공중/앉기/이동의도
	if (const UCharacterMovementComponent* MoveComp = OwningCharacter->GetCharacterMovement())
	{
		bIsInAir = MoveComp->IsFalling();
		bIsCrouched = MoveComp->IsCrouching();
		const bool bIsAccelerating = MoveComp->GetCurrentAcceleration().SizeSquared() > 0.f;
		bShouldMove = (GroundSpeed > 3.f) && bIsAccelerating;
	}

	// 3) 조준 + Aim Offset(상하/좌우) + 사격 (GAS)
	bIsAiming = OwningCharacter->IsCharacterAiming(); // 태그 State.Movement.Aiming

	// BaseAimRotation은 원격 폰 Pitch도 복제 → 멀티에서 상하 조준 정상
	const FRotator AimDelta = (OwningCharacter->GetBaseAimRotation() - OwningCharacter->GetActorRotation()).GetNormalized();
	AO_Yaw = AimDelta.Yaw;
	AO_Pitch = AimDelta.Pitch;

	if (UPFGASC* ASC = OwningCharacter->GetASC())
	{
		static const FGameplayTag FiringTag = FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing"));
		bIsFiring = ASC->HasAnyMatchingGameplayTags(FGameplayTagContainer(FiringTag));
	}

	// 4) 사망 판정 (HP 0 — 별도 사망 플래그가 없어 HP로)
	bIsDead = OwningCharacter->IsCharacterDeath();

	// 5) 현재 무기 타입 — 멀티 동기화의 진실원본인 PlayerState에서
	if (const AMainPlayerState* PS = OwningCharacter->GetPlayerState<AMainPlayerState>())
	{
		CurrentWeaponType = PS->GetWeaponID();
	}

	// 6) 무기 타입 → 스탠스(int): 0 맨손 / 1 권총(HG) / 2 양손총(그 외)
	switch (CurrentWeaponType)
	{
	case EWeaponType::None: WeaponStance = 0; break;
	case EWeaponType::PISTOL: WeaponStance = 1; break;
	default:                WeaponStance = 2; break;
	}

	// 7) 왼손 IK 타깃(World) — 장착 무기의 LeftHandGrip 소켓에 손바닥(LeftHand) 맞추도록 hand_l 보정.
	//    공식: Effector = 무기그립 + (hand_l − LeftHand). 무기 없으면 현재 hand_l(=IK 무효).
	if (USkeletalMeshComponent* MeshComp = OwningCharacter->GetMesh())
	{
		const FVector HandL = MeshComp->GetSocketLocation(FName("hand_l"));
		if (AWeapon* Gun = OwningCharacter->GetEquippedGun())
		{
			const FVector Grip = Gun->m_pMesh->GetSocketLocation(FName("LeftHandGrip"));
			const FVector Palm = MeshComp->GetSocketLocation(FName("LeftHand"));
			LeftIKLocation = Grip + (HandL - Palm);
		}
		else
		{
			LeftIKLocation = HandL; // 무기 없음 → 제자리 = IK no-op
		}
	}

	// 8) 재장전 감지 + 왼손 IK 알파.
	//    재장전 몽타주가 재생 중이면 왼손이 그립을 떠나므로 IK를 꺼야 한다(알파 0).
	bIsReloading = false;
	for (const TPair<EWeaponType, UAnimMontage*>& Pair : WeaponReloadMontages)
	{
		if (Pair.Value && Montage_IsPlaying(Pair.Value))
		{
			bIsReloading = true;
			break;
		}
	}
	// 왼손 IK는 양손총(WeaponStance==2)이고 재장전 중이 아닐 때만. 톡 끊기지 않게 부드럽게 보간.
	const bool bWantLeftIK = (bIsReloading == false) && (WeaponStance == 2);
	const float TargetAlpha = bWantLeftIK ? 1.f : 0.f;
	LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetAlpha, DeltaSeconds, 12.f);
}
void UMainAnimInstance::PlayFireMontage(EWeaponType WeaponType)
{
	UAnimMontage* TargetMontage = nullptr;
	if (WeaponFireMontages.Contains(WeaponType))
	{
		TargetMontage = WeaponFireMontages[WeaponType];
	}

	// [폴백 규칙] 지정된 몽타주가 없으면, 권총은 권총 몽타주로, 그 외(Shotgun, SMG, Sniper 등)는 AR(Rifle) 몽타주로 대체
	if (!TargetMontage)
	{
		EWeaponType FallbackType = (WeaponType == EWeaponType::PISTOL) ? EWeaponType::PISTOL : EWeaponType::AR;
		if (WeaponFireMontages.Contains(FallbackType))
		{
			TargetMontage = WeaponFireMontages[FallbackType];
		}
	}

	if (TargetMontage)
	{
		Montage_Play(TargetMontage);
	}
}

void UMainAnimInstance::PlayReloadMontage(EWeaponType WeaponType)
{
	UAnimMontage* TargetMontage = nullptr;
	if (WeaponReloadMontages.Contains(WeaponType))
	{
		TargetMontage = WeaponReloadMontages[WeaponType];
	}

	// [폴백 규칙] 지정된 몽타주가 없으면, 권총은 권총 몽타주로, 그 외(Shotgun, SMG, Sniper 등)는 AR(Rifle) 몽타주로 대체
	if (!TargetMontage)
	{
		EWeaponType FallbackType = (WeaponType == EWeaponType::PISTOL) ? EWeaponType::PISTOL : EWeaponType::AR;
		if (WeaponReloadMontages.Contains(FallbackType))
		{
			TargetMontage = WeaponReloadMontages[FallbackType];
		}
	}

	if (TargetMontage)
	{
		Montage_Play(TargetMontage);
	}
}

float UMainAnimInstance::GetReloadMontageLength(EWeaponType WeaponType) const
{
	UAnimMontage* TargetMontage = nullptr;
	if (WeaponReloadMontages.Contains(WeaponType))
	{
		TargetMontage = WeaponReloadMontages[WeaponType];
	}

	// [폴백 규칙] 지정된 몽타주가 없으면, 권총은 권총 몽타주로, 그 외(Shotgun, SMG, Sniper 등)는 AR(Rifle) 몽타주로 대체
	if (!TargetMontage)
	{
		EWeaponType FallbackType = (WeaponType == EWeaponType::PISTOL) ? EWeaponType::PISTOL : EWeaponType::AR;
		if (WeaponReloadMontages.Contains(FallbackType))
		{
			TargetMontage = WeaponReloadMontages[FallbackType];
		}
	}

	if (TargetMontage)
	{
		return TargetMontage->GetPlayLength();
	}
	return 0.f;
}