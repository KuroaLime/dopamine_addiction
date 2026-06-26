// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Crouch.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbility_Crouch::UAbility_Crouch()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Crouch")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Crouching")));
}

void UAbility_Crouch::LocalActivateWithOwner(AActor* InOwner)
{
	ApplyCrouch(InOwner, true);
}

void UAbility_Crouch::LocalCancelWithOwner(AActor* InOwner)
{
	ApplyCrouch(InOwner, false);
}

void UAbility_Crouch::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	ApplyCrouch(OwnerCharacter, true);
}

void UAbility_Crouch::EndAbility(bool bWasCancelled)
{
	ApplyCrouch(OwnerCharacter, false);
	Super::EndAbility(bWasCancelled);
}

void UAbility_Crouch::ApplyCrouch(AActor* InOwner, bool bWantCrouch)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	if (bWantCrouch)
	{
		// 기존 직접 호출과 동일하게 공중(낙하 중)에서는 앉지 않는다.
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && !Movement->IsFalling())
		{
			Character->Crouch();
		}
	}
	else
	{
		Character->UnCrouch();
	}
}
