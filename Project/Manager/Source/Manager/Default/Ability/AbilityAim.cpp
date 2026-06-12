// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityAim.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Data/CameraStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UAbilityAim::UAbilityAim()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming")));
}

void UAbilityAim::ActivateAbility()
{
    IAbilityOwnerInterface* Owner = GetOwnerInterface();
    if (!Owner) return;

    Owner->GetCameraStateComponent()->SmoothZoom(true);
    OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
    OwnerCharacter->bUseControllerRotationYaw = true;
}

void UAbilityAim::EndAbility(bool bWasCancelled)
{
    Super::EndAbility(bWasCancelled);

    IAbilityOwnerInterface* Owner = GetOwnerInterface();
    if (!Owner) return;

    Owner->GetCameraStateComponent()->SmoothZoom(false);
    OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = true;
    OwnerCharacter->bUseControllerRotationYaw = false;
}
