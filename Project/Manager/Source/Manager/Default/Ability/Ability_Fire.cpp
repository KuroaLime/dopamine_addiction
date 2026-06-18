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

	bIsServerFire = false;
	bIsClientFire = false;
}

void UAbility_Fire::LocalActivateWithOwner(AActor* InOwner)
{
	if (!bIsClientFire)
	{
		ACharacter* Character = Cast<ACharacter>(InOwner);
		if (!Character) return;
		IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
		IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());
		if (!PS_Interface || !GS_Interface) return;

		int32 FireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(),
			EWeaponBaseStatType::FireRate);

		// 발사 속도 하드 코딩
		float finalFireRate = FireRate * 0.01;

		FTimerDelegate Delegate;
		TWeakObjectPtr<AActor> WeakOwner(InOwner);
		Delegate.BindLambda([this, WeakOwner]()
			{
				if (!WeakOwner.IsValid()) return;
				Client_ExecuteFire(WeakOwner.Get());
			});

		Character->GetWorldTimerManager().SetTimer(
			ClientFireTimerHandle,
			Delegate,
			finalFireRate,
			true
		);
		bIsClientFire = true;

		Client_ExecuteFire(InOwner);
	}
}

void UAbility_Fire::LocalCancelWithOwner(AActor* InOwner)
{
	if (!InOwner) return;
	InOwner->GetWorldTimerManager().ClearTimer(ClientFireTimerHandle);
	bIsClientFire = false;
}

void UAbility_Fire::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	if (!bIsServerFire)
	{
		IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
		IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
		IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

		if (!Owner || !PS_Interface || !GS_Interface) return;

		int32 FireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(), 
			EWeaponBaseStatType::FireRate);

		// 발사 속도 하드 코딩
		float finalFireRate = FireRate * 0.01;

		bIsServerFire = true;
		OwnerCharacter->GetWorldTimerManager().SetTimer(
			ServerFireTimerHandle,
			this,
			&UAbility_Fire::Server_ExecuteFire,
			finalFireRate,
			true
		);

		bIsServerFire = true;

		Server_ExecuteFire();
	}
}

void UAbility_Fire::EndAbility(bool bWasCancelled)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);
	bIsServerFire = false;
	Super::EndAbility(bWasCancelled);
}

void UAbility_Fire::Client_ExecuteFire(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

	// 발사 이펙트/사운드 재생
	EquippedGun->Setting->Fire(MuzzleLoc);
}

void UAbility_Fire::Server_ExecuteFire()
{
	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface) return;

	UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
	if (!FollowCamera) return;

	int32 BaseRange = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Range);
	int32 RangeUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float FinalRange = CalculateRange(BaseRange, RangeUpgradeLevel);

	FVector CamStart = FollowCamera->GetComponentLocation();
	FRotator CamRot = FollowCamera->GetComponentRotation();
	FVector CamEnd = CamStart + (CamRot.Vector() * FinalRange);

	FHitResult CamHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);

	UWorld* World = OwnerCharacter->GetWorld();
	bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(
		PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
	int32 DamageUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float FinalDamage = CalculateDamage(BaseDamage, DamageUpgradeLevel);

	if (!bCamHit || !CamHit.GetActor()) return;

	UGameplayStatics::ApplyDamage(
		CamHit.GetActor(),
		FinalDamage,
		OwnerCharacter ? OwnerCharacter->GetController() : nullptr,
		OwnerCharacter,
		nullptr
	);
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
