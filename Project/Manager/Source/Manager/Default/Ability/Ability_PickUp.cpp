// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_PickUp.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/Card/CardGameService.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

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

	// 이미 서버 권위 컨텍스트이므로 컨트롤러 Server RPC를 거치지 않고 카드 서비스를 직접 호출한다.
	// (Ability_Death가 GetCardGameService()를 직접 쓰는 패턴과 동일)
	if (AMainPlayerController* PC = Cast<AMainPlayerController>(OwnerCharacter->GetController()))
	{
		if (!PC->TryConsumeCardPickupRequest())
		{
			EndAbilityNow();
			return;
		}

		if (UWorld* World = OwnerCharacter->GetWorld())
		{
			if (AMainGameMode* GM = World->GetAuthGameMode<AMainGameMode>())
			{
				if (UCardGameService* Cards = GM->GetCardGameService())
				{
					Cards->TryPickupNearestCard(PC);
				}
			}
		}
	}

	EndAbilityNow();
}

void UAbility_PickUp::EndAbility(bool bWasCancelled)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	Super::EndAbility(bWasCancelled);
}
