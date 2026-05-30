// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityAim.h"
#include "Game/InGame/TPS/System/TPSCharacter.h"
#include "Default/Data/CameraStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"

UAbilityAim::UAbilityAim()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming")));
}

void UAbilityAim::ActivateAbility()
{
    ATPSCharacter* MC = Cast<ATPSCharacter>(OwnerCharacter);

    if (MC)
    {
        MC->CameraState->SmoothZoom(true);
        MC->GetCharacterMovement()->bOrientRotationToMovement = false;
        MC->bUseControllerRotationYaw = true;
    }
}

void UAbilityAim::EndAbility(bool bWasCancelled)
{
    Super::EndAbility(bWasCancelled);

    ATPSCharacter* MC = Cast<ATPSCharacter>(OwnerCharacter);
    if (MC)
    {
        MC->CameraState->SmoothZoom(false);
        MC->GetCharacterMovement()->bOrientRotationToMovement = true;
        MC->bUseControllerRotationYaw = false;
    }
}
