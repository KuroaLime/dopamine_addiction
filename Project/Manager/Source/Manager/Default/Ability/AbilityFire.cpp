
#include "Default/Ability/AbilityFire.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Ability/CustomASC.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h" // [추가] 카메라 컴포넌트 헤더 추가

UAbilityFire::UAbilityFire()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

    CooldownDuration = 0.5f;
    CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));
}

void UAbilityFire::ActivateAbility()
{
    IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
    if (!Owner) return;

    AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
    UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
    if (!EquippedGun || !FollowCamera) return;

    // 카메라 기준 레이캐스트
    FVector CamStart = FollowCamera->GetComponentLocation();
    FRotator CamRot = FollowCamera->GetComponentRotation();
    FVector CamEnd = CamStart + (CamRot.Vector() * 10000.f);

    FHitResult CamHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    bool bCamHit = GetWorld()->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);
    FVector TargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;

    // 총구 위치
    FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

    // 근거리/원거리 분기
    float Distance = FVector::Dist(MuzzleLoc, TargetPoint);
    if (Distance < 200.0f)
    {
        FVector FinalEnd = MuzzleLoc + (EquippedGun->GetActorForwardVector() * 10000.f);
        EquippedGun->Setting->Fire(MuzzleLoc, FinalEnd);
    }
    else
    {
        EquippedGun->Setting->Fire(MuzzleLoc, TargetPoint);
    }

    if (bCamHit && OwnerCharacter->IsLocallyControlled())
    {
        OwnerASC->ServerRPC_ProcessHit(CamHit);
    }

    EndAbility(false);
}