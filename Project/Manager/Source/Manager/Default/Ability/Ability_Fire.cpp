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
#include "Game/InGame/MainGameMode.h"

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

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

		float finalFireRate = FireRate * 0.01f;

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

		float finalFireRate = FireRate * 0.01f;

		bIsServerFire = true;
		OwnerCharacter->GetWorldTimerManager().SetTimer(
			ServerFireTimerHandle,
			this,
			&UAbility_Fire::Server_ExecuteFire,
			finalFireRate,
			true
		);

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
	EquippedGun->Setting->Fire(MuzzleLoc);
}

void UAbility_Fire::Server_ExecuteFire()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		EndAbilityNow();
		return;
	}

	UWorld* World = OwnerCharacter->GetWorld();
	AMainGameMode* GM = World ? World->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM || !GM->IsBattleRoyalePhase())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=InvalidPhase Owner=%s"), *OwnerCharacter->GetName());
		EndAbilityNow();
		return;
	}

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = World ? Cast<IPhaseGameStateInterface>(World->GetGameState()) : nullptr;

	if (!Owner || !PS_Interface || !GS_Interface)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=MissingInterface Owner=%s"), *OwnerCharacter->GetName());
		EndAbilityNow();
		return;
	}

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=NoWeapon Owner=%s"), *OwnerCharacter->GetName());
		EndAbilityNow();
		return;
	}

	int32 BaseRange = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Range);
	int32 RangeUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float FinalRange = CalculateRange(BaseRange, RangeUpgradeLevel);

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
	int32 DamageUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float FinalDamage = CalculateDamage(BaseDamage, DamageUpgradeLevel);

	FVector ViewLocation = OwnerCharacter->GetActorLocation();
	FRotator ViewRotation = OwnerCharacter->GetActorRotation();
	if (AController* Controller = OwnerCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	FVector TargetPoint = ViewLocation + (ViewRotation.Vector() * FinalRange);
	FVector MuzzleLoc = EquippedGun->m_pMesh ? EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle")) : OwnerCharacter->GetActorLocation();

	UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireAccepted Owner=%s WeaponID=%d Damage=%.2f Range=%.2f"),
		*OwnerCharacter->GetName(),
		static_cast<int32>(PS_Interface->GetWeaponID()),
		FinalDamage,
		FinalRange);

	EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint, FinalDamage);

	EndAbilityNow();
}

bool UAbility_Fire::TryActivateAbilityWithEvent(const FPFGGameplayEventData& Payload)
{
	return false;
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
