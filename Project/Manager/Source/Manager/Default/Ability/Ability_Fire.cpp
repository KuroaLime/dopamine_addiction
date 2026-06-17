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
#include "Game/InGame/MainGameMode.h"

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

	// TriggerTags: HandleGameplayEvent("Event.Weapon.Hit")로 데미지 어빌리티가 발동됨

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

	if (!Owner) return;

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

	// 발사 이펙트/사운드 재생
	EquippedGun->Setting->Fire(MuzzleLoc);

}

void UAbility_Fire::ActivateAbility()
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
