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
#include "Game/InGame/MainGameMode.h"
#include "GameFramework/Controller.h"

UAbilityFire::UAbilityFire()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

    CooldownDuration = 0.5f;
    CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));
}

void UAbilityFire::ActivateAbility()
{
    ExecuteServerFire(nullptr);
}

bool UAbilityFire::TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
    return ExecuteServerFire(&Payload);
}

bool UAbilityFire::ExecuteServerFire(const FCustomGameplayEventData* Payload)
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
    {
        EndAbility(true);
        return false;
    }

    AMainGameMode* MainGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!MainGM || !MainGM->IsBattleRoyalePhase())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=InvalidPhase Owner=%s"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("<NULL>"));
        EndAbility(true);
        return false;
    }

    IAbilityOwnerInterface* Owner = GetOwnerInterface();
    IPhasePlayerStateInterface* PS_Interface = GetPSInterface();
    IPhaseGameStateInterface* GS_Interface = GetGSInterface();
    if (!Owner || !PS_Interface || !GS_Interface)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=MissingInterface Owner=%s"),
            *OwnerCharacter->GetName());
        EndAbility(true);
        return false;
    }

    AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
    if (!EquippedGun || !EquippedGun->Setting || !EquippedGun->m_pMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=MissingWeapon Owner=%s"),
            *OwnerCharacter->GetName());
        EndAbility(true);
        return false;
    }

    int32 BaseRange = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Range);
    int32 RangeUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
    float FinalRange = CalculateRange(BaseRange, RangeUpgradeLevel);

    int32 BaseDamage = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
    int32 DamageUpgradeLevel = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
    float FinalDamage = CalculateDamage(BaseDamage, DamageUpgradeLevel);

    AController* Controller = OwnerCharacter->GetController();
    if (!Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=MissingController Owner=%s"),
            *OwnerCharacter->GetName());
        EndAbility(true);
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    bool bUsedClientAim = false;

    if (Payload && Payload->bHasAimData)
    {
        const float MaxAimAnchorDistance = 1500.0f;
        if (FVector::DistSquared(Payload->AimLocation, OwnerCharacter->GetActorLocation()) <= FMath::Square(MaxAimAnchorDistance))
        {
            ViewLocation = Payload->AimLocation;
            ViewRotation = Payload->AimRotation;
            bUsedClientAim = true;
        }
    }

    if (!bUsedClientAim)
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ViewEnd = ViewLocation + (ViewRotation.Vector() * FinalRange);

    FHitResult ViewHit;
    FCollisionQueryParams ViewParams(SCENE_QUERY_STAT(ServerFireViewTrace), false);
    ViewParams.AddIgnoredActor(OwnerCharacter);
    ViewParams.AddIgnoredActor(EquippedGun);

    const bool bViewHit = GetWorld()->LineTraceSingleByChannel(ViewHit, ViewLocation, ViewEnd, ECC_Visibility, ViewParams);
    const FVector TargetPoint = bViewHit ? ViewHit.ImpactPoint : ViewEnd;
    const FVector MuzzleLoc = EquippedGun->m_pMesh->DoesSocketExist(TEXT("Muzzle"))
        ? EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"))
        : EquippedGun->GetActorLocation();

    UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireAccepted Owner=%s WeaponID=%d Damage=%.2f Range=%.2f ClientAim=%d ViewLoc=%s ViewRot=%s Target=%s ViewHit=%s"),
        *OwnerCharacter->GetName(),
        static_cast<int32>(PS_Interface->GetWeaponID()),
        FinalDamage,
        FinalRange,
        bUsedClientAim ? 1 : 0,
        *ViewLocation.ToCompactString(),
        *ViewRotation.ToCompactString(),
        *TargetPoint.ToCompactString(),
        bViewHit && ViewHit.GetActor() ? *ViewHit.GetActor()->GetName() : TEXT("<None>"));

    EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint, FinalDamage);
    EndAbility(false);
    return true;
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
