// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Shop.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/GameStateBase.h"
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
	{
		IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());
		if (GS && GS->IsShopAvailable())
		{
			PC->PushMode(EGamePhase::Shop);
		}
		break;
	}
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