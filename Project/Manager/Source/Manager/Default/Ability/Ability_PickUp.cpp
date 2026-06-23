// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_PickUp.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UAbility_PickUp::UAbility_PickUp()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.PickUp")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.PickUp")));
}

void UAbility_PickUp::LocalActivateWithOwner(AActor* InOwner)
{

}

void UAbility_PickUp::LocalCancelWithOwner(AActor* InOwner)
{

}

void UAbility_PickUp::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	IPhasePlayerControllerInterface* PC_Interface =
		Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());

	if (!PC_Interface) return;

	PC_Interface->PickupNearestCard();

	EndAbilityNow();
}

void UAbility_PickUp::EndAbility(bool bWasCancelled)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	Super::EndAbility(bWasCancelled);
}