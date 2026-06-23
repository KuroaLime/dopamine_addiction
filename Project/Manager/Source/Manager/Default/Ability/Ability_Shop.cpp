// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Shop.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UAbility_Shop::UAbility_Shop()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));

	bLocalOnly = true;
}

void UAbility_Shop::LocalActivateWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IPhasePlayerControllerInterface* PC =
		Cast<IPhasePlayerControllerInterface>(Character->GetController());
	if (!PC) return;

	EGamePhase CurrentPhase = PC->GetCurrentPhase();
	switch (CurrentPhase)
	{
	case EGamePhase::TPS:
		PC->PushMode(EGamePhase::Shop);
		break;
	case EGamePhase::Shop:
		PC->PopMode();
		break;
	default:
		break;
	}
}

void UAbility_Shop::ActivateAbility()
{
	EndAbilityNow();
}