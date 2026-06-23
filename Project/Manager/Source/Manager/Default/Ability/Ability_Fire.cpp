#include "Default/Ability/Ability_Fire.h"
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
#include "DrawDebugHelpers.h"

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

	CooldownDuration = 0.0;
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



		int32 BaseFireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(),
			EWeaponBaseStatType::FireRate);
		int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);

		float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);

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
			FireRate,
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


		int32 BaseFireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(),
			EWeaponBaseStatType::FireRate);
		int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);

		float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);


		bIsServerFire = true;
		OwnerCharacter->GetWorldTimerManager().SetTimer(
			ServerFireTimerHandle,
			this,
			&UAbility_Fire::Server_ExecuteFire,
			FireRate,
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
	if (!EquippedGun || !EquippedGun->Setting || !EquippedGun->m_pMesh) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
	//EquippedGun->Setting->Fire(MuzzleLoc);
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
	int32 LvRange = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float Range = CalculateRange(BaseRange, LvRange);


	FVector CamStart = FollowCamera->GetComponentLocation();
	FRotator CamRot = FollowCamera->GetComponentRotation();
	FVector CamEnd = CamStart + (CamRot.Vector() * Range);

	FHitResult CamHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);




	UWorld* World = OwnerCharacter->GetWorld();
	bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);


	// -------------------------------------------------------
	// 디버그 표시
	//   초록: 히트   (라인 CamStart → ImpactPoint + 박스)
	//   빨강: 미스   (라인 CamStart → CamEnd)
	// -------------------------------------------------------
	if (bCamHit && CamHit.GetActor())
	{
		DrawDebugLine(World, CamStart, CamHit.ImpactPoint, FColor::Green, false, 2.0f, 0, 0.5f);
		DrawDebugBox(World, CamHit.ImpactPoint, FVector(15.0f), FColor::Green, false, 2.0f, 0, 2.0f);
	}
	else
	{
		DrawDebugLine(World, CamStart, CamEnd, FColor::Red, false, 2.0f, 0, 1.5f);
	}

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting || !EquippedGun->m_pMesh) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
	EquippedGun->Setting->Multicast_PlayFireFeedback(MuzzleLoc);

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(
		PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
	int32 LvDamage = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float Damage = CalculateDamage(BaseDamage, LvDamage);

	if (!bCamHit || !CamHit.GetActor()) return;

	UGameplayStatics::ApplyDamage(
		CamHit.GetActor(),
		Damage,
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

float UAbility_Fire::CalculateFireRate(int32 Base, int32 Level) const
{
	float BaseDelay = Base * 0.1f;
	float FireRate = BaseDelay;
	float SpeedBonus = (Level * 0.1f) + Base;
	FireRate = BaseDelay / (1.f + SpeedBonus);

	return FireRate;
}