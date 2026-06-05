// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityShop.h"
#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "GameFramework/Character.h" 

UAbilityShop::UAbilityShop()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));

}

void UAbilityShop::ActivateAbility()
{

	if (OwnerCharacter)
	{

		
		APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
		AActor* TargetActor = PC ? Cast<AActor>(PC) : Cast<AActor>(OwnerCharacter);

		if (TargetActor)
		{
			TArray<UUIHandler*> UIHandlers;
			TargetActor->GetComponents<UUIHandler>(UIHandlers);

			UUIHandler* ShopHandler = nullptr;
			for (UUIHandler* Handler : UIHandlers)
			{
				if (Handler->ComponentHasTag(FName("Shop")))
				{
					ShopHandler = Handler;
					break;
				}
			}

			if (ShopHandler)
			{
				 ShopHandler->UIToggle();

			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AbilityShop: 'Shop' 태그를 가진 UIHandler 컴포넌트를 찾을 수 없습니다."));
			}
		}
	}

	EndAbility(false);
}