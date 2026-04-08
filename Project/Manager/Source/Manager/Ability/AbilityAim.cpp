// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityAim.h"
#include "ManagerCharacter.h"
#include "Data/CameraStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/WeaponComponent.h"

UAbilityAim::UAbilityAim()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming")));
}

void UAbilityAim::ActivateAbility()
{
    AManagerCharacter* MC = Cast<AManagerCharacter>(OwnerCharacter);
    if (MC)
    {
       MC->CameraState->SmoothZoom(true);
        MC->GetCharacterMovement()->bOrientRotationToMovement = false;
        MC->bUseControllerRotationYaw = true;
    }
}

void UAbilityAim::EndAbility(bool bWasCancelled)
{
    // [일관성] 어빌리티가 끝날 때(버튼 뗄 때) 캐릭터에게 줌아웃(false)을 명령합니다.
    Super::EndAbility(bWasCancelled); // 태그 제거 로직 실행

    AManagerCharacter* MC = Cast<AManagerCharacter>(OwnerCharacter);
    if (MC)
    {
        MC->CameraState->SmoothZoom(false);
        MC->GetCharacterMovement()->bOrientRotationToMovement = true;
        MC->bUseControllerRotationYaw = false;
    }
}
