
#include "Default/Ability/AbilityFire.h"
#include "Default/Ability/CustomASC.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h" 
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

UAbilityFire::UAbilityFire()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

    CooldownDuration = 0.5f;
    CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));
}

void UAbilityFire::ActivateAbility()
{
    IAbilityOwnerInterface* Owner = GetOwnerInterface();
    IPhasePlayerStateInterface* PS_Interface = GetPSInterface();
    IPhaseGameStateInterface* GS_Interface = GetGSInterface();
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
    Params.AddIgnoredActor(OwnerCharacter);

    bool bCamHit = GetWorld()->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);
    FVector TargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;
    FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
    DrawDebugLine(GetWorld(), MuzzleLoc, TargetPoint, FColor::Red, false, 1.0f, 0, 1.0f);

    if (bCamHit)
    {
        AActor* HitActor = CamHit.GetActor();
        if (HitActor)
        {
            DrawDebugBox(GetWorld(), CamHit.ImpactPoint, FVector(7, 7, 7), FColor::Green, false, 1.0f);
        }
    }

    /*float Distance = FVector::Dist(MuzzleLoc, TargetPoint);
    if (Distance < 200.0f)
    {
        FVector FinalEnd = MuzzleLoc + (EquippedGun->GetActorForwardVector() * FinalRange);
        EquippedGun->Setting->Fire(MuzzleLoc, FinalEnd);
    }
    else
    {
        EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint);
    }*/

    // 무조건 총기 발사 애니메이션 및 소리 나게 함
    EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint);

    if (bCamHit && OwnerCharacter->IsLocallyControlled())
    {
        FCustomGameplayEventData Payload;
        Payload.Instigator = OwnerCharacter;
        Payload.TargetObject = CamHit.GetActor();

        static const FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(FName("Event.Weapon.Hit"));
        OwnerASC->ServerRPC_SendGameplayEvent(HitTag, Payload);
    }

    EndAbility(false);
}

bool UAbilityFire::TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
    AActor* HitActor = Payload.TargetObject;
    if (!HitActor) return false;

    IPhasePlayerStateInterface* PS_Interface = GetPSInterface();
    IPhaseGameStateInterface* GS_Interface = GetGSInterface();

    if (PS_Interface && GS_Interface)
    {
        int32 BaseDamage = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
        int32 DamageUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);

        float FinalDamage = CalculateDamage(BaseDamage, DamageUpgradeLevel);

        UGameplayStatics::ApplyDamage(
            HitActor,
            FinalDamage,
            OwnerCharacter->GetController(),
            OwnerCharacter,
            nullptr
        );
        return true;
    }

    return false;
}

float UAbilityFire::CalculateDamage(const int32& Base, const int32& Level)
{
    return static_cast<float>(Base) + (Level * 5.0f);
}

float UAbilityFire::CalculateRange(const int32& Base, const int32& Level)
{
    float range = static_cast<float>(Base);
    return range + (range * (Level * 0.1f));
}