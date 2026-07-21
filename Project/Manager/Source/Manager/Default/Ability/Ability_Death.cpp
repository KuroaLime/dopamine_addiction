// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/Ability/Ability_Death.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/MainPlayerState.h"
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

	DropDeathGold();

	RespawnTime = 5;

	AController* CharController = OwnerCharacter->GetController();
	IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(CharController);
	if (!PC)
	{
		EndAbilityNow();
		return;
	}

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

void UAbility_Death::DropDeathGold()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC)
	{
		return;
	}

	AMainPlayerState* PlayerState = Cast<AMainPlayerState>(PC->PlayerState);
	if (!PlayerState)
	{
		return;
	}

	AMainGameMode* GameMode = Cast<AMainGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->IsBattleRoyalePhase())
	{
		return;
	}

	GameMode->DropGoldFromPlayer(PlayerState, OwnerCharacter);
}

void UAbility_Death::Server_ExecuteCountDown()
{
	AController* CharController = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;
	IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(CharController);
	
	if (!PC)
	{
		if (OwnerCharacter)
		{
			OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerRespawnTimerHandle);
		}
		EndAbilityNow();
		return;
	}

	RespawnTime--;

	if (RespawnTime <= 0)
	{
		OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerRespawnTimerHandle);

		EndAbilityNow();

		static const FGameplayTag RespawnTag =
			FGameplayTag::RequestGameplayTag(FName("Ability.Action.Respawn"));

		if (OwnerASC)
		{
			OwnerASC->TryActivateAbilityByTag(RespawnTag);
		}
		return;
	}

	PC->SetUITimer(RespawnTime);
}
