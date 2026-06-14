// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Jump.h"
#include "GameFramework/Character.h"

UAbility_Jump::UAbility_Jump()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Jump")));
}

void UAbility_Jump::LocalActivateWithOwner(AActor* InOwner)
{
	if (ACharacter* Character = Cast<ACharacter>(InOwner))
	{
		Character->Jump();
	}
}

void UAbility_Jump::ActivateAbility()
{
	EndAbilityNow();
}