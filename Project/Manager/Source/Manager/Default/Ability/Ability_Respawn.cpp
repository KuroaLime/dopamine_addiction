// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Respawn.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UAbility_Respawn::UAbility_Respawn()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Respawn")));
}

void UAbility_Respawn::LocalActivateWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IPhasePlayerControllerInterface* PC =
		Cast<IPhasePlayerControllerInterface>(Character->GetController());
	if (!PC) return;

	PC->SwitchState(EGamePhase::TPS);
}

void UAbility_Respawn::LocalCancelWithOwner(AActor* InOwner)
{

}

void UAbility_Respawn::ActivateAbility()
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
    {
        EndAbilityNow();
        return;
    }

    IPhasePlayerControllerInterface* PC =
        Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());
    if (!PC)
    {
        EndAbilityNow();
        return;
    }

    IPhasePlayerStateInterface* PS =
        Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
    if (PS) PS->ResetState();

    PC->SwitchState(EGamePhase::TPS);

	EndAbilityNow();
}

void UAbility_Respawn::EndAbility(bool bWasCancelled)
{
	Super::EndAbility(bWasCancelled);
}