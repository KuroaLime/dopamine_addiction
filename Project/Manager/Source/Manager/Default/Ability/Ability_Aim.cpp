// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Aim.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Data/CameraStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UAbility_Aim::UAbility_Aim()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming")));
}

void UAbility_Aim::LocalActivateWithOwner(AActor* InOwner)
{
	RestoreLocalState(InOwner, true);
}

void UAbility_Aim::LocalCancelWithOwner(AActor* InOwner)
{
	RestoreLocalState(InOwner, false);
}


void UAbility_Aim::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	OwnerCharacter->bUseControllerRotationYaw = true;
	OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
}

void UAbility_Aim::EndAbility(bool bWasCancelled)
{
	Super::EndAbility(bWasCancelled);
}

void UAbility_Aim::RestoreLocalState(AActor* InOwner, bool isAim)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	if (UCameraStateComponent* CamState = Owner->GetCameraStateComponent())
	{
		CamState->SmoothZoom(isAim);
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = !isAim;
	Character->bUseControllerRotationYaw = isAim;
}