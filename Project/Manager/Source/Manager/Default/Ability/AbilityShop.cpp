// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityShop.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "GameFramework/Character.h" 
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

UAbilityShop::UAbilityShop()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));

}

void UAbilityShop::ActivateAbility()
{
    if (IPhasePlayerControllerInterface* PC = GetPCInterface())
    {
        EGamePhase CurrentPhase = PC->GetCurrentPhase();

        switch (CurrentPhase)
        {
        case EGamePhase::TPS:
            PC->PushMode(EGamePhase::Shop);
            break;
        case EGamePhase::Shop:
            PC->PopMode();
            break;
        default: break;
        }
    }

	EndAbility(false);
}