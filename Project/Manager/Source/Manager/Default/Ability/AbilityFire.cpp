
#include "Default/Ability/AbilityFire.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Default/Ability/CustomASC.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
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
    AManagerCharacter* MC = Cast<AManagerCharacter>(OwnerCharacter);
    if (!MC || !MC->GetEquippedGun()) return;

    // [수정된 부분 1] GetActorEyesViewPoint 대신, 실제 3인칭 FollowCamera의 트랜스폼을 가져옵니다.
    FVector CamStart = MC->GetFollowCamera()->GetComponentLocation();
    FRotator CamRot = MC->GetFollowCamera()->GetComponentRotation();
    FVector CamEnd = CamStart + (CamRot.Vector() * 10000.f);

    FHitResult CamHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter); // 본인 제외

    // [수정된 부분 2] ECC_Pawn에서 ECC_Visibility로 변경하여 맵(벽/바닥)도 인식하게 합니다.
    bool bCamHit = GetWorld()->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);
    FVector TargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;

    // 2. 총구 위치 가져오기
    FVector MuzzleLoc = MC->GetEquippedGun()->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

    // 3. 거리 계산 (근거리 판단)
    float Distance = FVector::Dist(MuzzleLoc, TargetPoint);

    if (Distance < 200.0f) {
        // [근거리] 총구 정면 방향으로 MaxRange만큼 떨어진 '좌표'를 계산해서 넘깁니다.
        FVector MuzzleForward = MC->GetEquippedGun()->GetActorForwardVector();
        FVector FinalEnd = MuzzleLoc + (MuzzleForward * 10000.f); // 방향이 아니라 도착점 좌표
        MC->GetEquippedGun()->Setting->Fire(MuzzleLoc, FinalEnd);
    }
    else {
        // [원거리] 이미 좌표인 TargetPoint를 그대로 넘깁니다.
        MC->GetEquippedGun()->Setting->Fire(MuzzleLoc, TargetPoint);
    }

    if (bCamHit && OwnerCharacter->IsLocallyControlled()) {
        OwnerASC->ServerRPC_ProcessHit(CamHit);
    }

    EndAbility(false);
}