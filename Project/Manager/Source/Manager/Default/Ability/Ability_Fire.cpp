// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Fire.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

	// TriggerTags: HandleGameplayEvent("Event.Weapon.Hit")로 데미지 어빌리티가 발동됨
	TriggerTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Event.Weapon.Hit")));

	CooldownDuration = 0.5f;
	CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));
}

void UAbility_Fire::LocalActivateWithOwner(AActor* InOwner)
{
	// 클라이언트 전용: 레이캐스트 + 발사 이펙트 + 히트 시 ServerRPC 전송.
	// 서버 응답을 기다리지 않고 즉시 실행해 발사 반응을 자연스럽게 만든다.
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	// 로컬 소유 클라이언트에서만 실행
	if (!Character->IsLocallyControlled()) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface) return;

	int32 BaseRange = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Range);
	int32 RangeUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float FinalRange = CalculateRange(BaseRange, RangeUpgradeLevel);

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
	if (!EquippedGun || !FollowCamera) return;

	FVector CamStart = FollowCamera->GetComponentLocation();
	FRotator CamRot = FollowCamera->GetComponentRotation();
	FVector CamEnd = CamStart + (CamRot.Vector() * FinalRange);

	FHitResult CamHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	UWorld* World = Character->GetWorld();
	bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);
	FVector TargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;
	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

	DrawDebugLine(World, MuzzleLoc, TargetPoint, FColor::Red, false, 1.0f, 0, 1.0f);

	if (bCamHit && CamHit.GetActor())
	{
		DrawDebugBox(World, CamHit.ImpactPoint, FVector(7, 7, 7), FColor::Green, false, 1.0f);
	}

	// 발사 이펙트/사운드 재생
	EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint);

	// 히트 발생 시 서버에 이벤트 전송 -> 서버가 데미지를 적용
	if (bCamHit && CamHit.GetActor())
	{
		if (UPFGASC* ASC = Cast<UPFGASC>(Character->GetComponentByClass(UPFGASC::StaticClass())))
		{
			FPFGGameplayEventData Payload;
			Payload.Instigator = Character;
			Payload.TargetObject = CamHit.GetActor();

			static const FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(FName("Event.Weapon.Hit"));
			ASC->ServerRPC_SendGameplayEvent(HitTag, Payload);
		}
	}
}

void UAbility_Fire::ActivateAbility()
{
	// 서버: 실제 발사 로직은 클라이언트의 LocalActivateWithOwner에서 처리.
	// 여기서는 쿨다운/ActivationOwnedTags가 TryActivateAbility에서 자동 처리되므로
	// 즉시 종료만 한다.
	EndAbilityNow();
}

bool UAbility_Fire::TryActivateAbilityWithEvent(const FPFGGameplayEventData& Payload)
{
	// 서버 전용: ServerRPC_SendGameplayEvent("Event.Weapon.Hit")를 받아 실제 데미지 적용.
	// CanExecute(쿨다운/BlockedTags)는 HandleGameplayEvent에서 이미 통과한 상태.
	AActor* HitActor = Payload.TargetObject;
	if (!HitActor) return false;

	IPhasePlayerStateInterface* PS_Interface = GetPSInterface();
	IPhaseGameStateInterface* GS_Interface = GetGSInterface();
	if (!PS_Interface || !GS_Interface) return false;

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(
		PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
	int32 DamageUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float FinalDamage = CalculateDamage(BaseDamage, DamageUpgradeLevel);

	UGameplayStatics::ApplyDamage(
		HitActor,
		FinalDamage,
		OwnerCharacter ? OwnerCharacter->GetController() : nullptr,
		OwnerCharacter,
		nullptr
	);

	return true;
}

float UAbility_Fire::CalculateDamage(int32 Base, int32 Level) const
{
	return static_cast<float>(Base) + (Level * 5.0f);
}

float UAbility_Fire::CalculateRange(int32 Base, int32 Level) const
{
	float Range = static_cast<float>(Base);
	return Range + (Range * (Level * 0.1f));
}
