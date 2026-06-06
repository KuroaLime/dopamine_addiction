// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityShop.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "GameFramework/Character.h" 

UAbilityShop::UAbilityShop()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));

}

void UAbilityShop::ActivateAbility()
{
    if (OwnerCharacter)
    {
        AController* Ctrl = OwnerCharacter->GetController();

        if (IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(Ctrl))
        {
            EGamePhase CurrentPhase = PC->GetCurrentPhase();
            switch (CurrentPhase)
            {
            case EGamePhase::TPS:
                PC->SwitchMode(EGamePhase::Shop);
                break;
            case EGamePhase::Shop:  
                PC->SwitchMode(EGamePhase::TPS);
                break;
            default: break;
            }
        }
    }

	EndAbility(false);
}