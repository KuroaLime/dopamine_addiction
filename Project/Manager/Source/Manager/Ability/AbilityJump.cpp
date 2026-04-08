// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityJump.h"
#include "GameFramework/Character.h" 

UAbilityJump::UAbilityJump()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Jump")));

	CooldownDuration = 0.5f;
	CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Jump")));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Jumping")));
}

void UAbilityJump::ActivateAbility()
{
	if (OwnerCharacter)
	{
		OwnerCharacter->Jump();
	}
	EndAbility(false);
}