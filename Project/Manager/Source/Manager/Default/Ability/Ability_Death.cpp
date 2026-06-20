// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Death.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UAbility_Death::UAbility_Death()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Death")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Death")));
}

void UAbility_Death::LocalActivateWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IPhasePlayerControllerInterface* PC =
		Cast<IPhasePlayerControllerInterface>(Character->GetController());
	if (!PC) return;

	PC->SwitchState(EGamePhase::Death);
}

void UAbility_Death::LocalCancelWithOwner(AActor* InOwner)
{

}

void UAbility_Death::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		EndAbilityNow(); 
		return;
	}

	RespawnTime = 5;

	IPhasePlayerControllerInterface* PC =
		Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());
	if (!PC) return;

	PC->SetUITimer(RespawnTime);

	OwnerCharacter->GetWorldTimerManager().SetTimer(
		ServerRespawnTimerHandle,
		this,
		&UAbility_Death::Server_ExecuteCountDown,
		1.f,
		true
	);
}

void UAbility_Death::EndAbility(bool bWasCancelled)
{
	Super::EndAbility(bWasCancelled);
}

void UAbility_Death::Server_ExecuteCountDown()
{
	IPhasePlayerControllerInterface* PC =
		Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());
	if (!PC) return;

	RespawnTime--;

	if (RespawnTime <= 0)
	{
		OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerRespawnTimerHandle);

		EndAbilityNow();

		static const FGameplayTag RespawnTag =
			FGameplayTag::RequestGameplayTag(FName("Ability.Action.Respawn"));

		EPFGAbilityActivationResult Result = OwnerASC->TryActivateAbilityByTag(RespawnTag);
	}

	PC->SetUITimer(RespawnTime);
}