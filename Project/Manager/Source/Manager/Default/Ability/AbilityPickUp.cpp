// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityPickUp.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Default/Actor/BaseItem.h"

UAbilityPickUp::UAbilityPickUp()
{
	// [중요] 이 스킬의 이름표(Tag) 설정
	// 캐릭터가 F키를 누르면 이 태그를 찾아서 실행합니다.
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.PickUp")));
}

void UAbilityPickUp::ActivateAbility()
{
	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);
	if (!Character)
	{
		EndAbility(true);
		return;
	}

	// (이미 들고 있는지 검사 로직 생략...)

	// 주변 탐색 로직
	FVector Start = Character->GetActorLocation();
	FCollisionShape Sphere = FCollisionShape::MakeSphere(150.0f);
	TArray<FHitResult> OutHits;
	bool bHit = GetWorld()->SweepMultiByChannel(OutHits, Start, Start, FQuat::Identity, ECC_PhysicsBody, Sphere);

	if (bHit)
	{
		for (const FHitResult& Result : OutHits)
		{
			if (ABaseItem* FoundItem = Cast<ABaseItem>(Result.GetActor()))
			{
				PendingItem = FoundItem;

				if (MontageToPlay)
				{
					float Duration = Character->PlayAnimMontage(MontageToPlay);
					FTimerHandle TimerHandle;
					GetWorld()->GetTimerManager().SetTimer(
						TimerHandle,
						this,
						&UAbilityPickUp::RealPickUp, 
						Duration,                 
						false                   
					);

					return;
				}
				else
				{
					RealPickUp();
					return;
				}
			}
		}
	}

	EndAbility(true);
}

void UAbilityPickUp::RealPickUp()
{
	// 타이머 시간이 다 돼서 여기가 실행됨

	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);
	if (Character && PendingItem && PendingItem->IsValidLowLevel())
	{
		PendingItem->OnPickedUp(Character);
	}

	PendingItem = nullptr;

	// 모든 게 끝났으니 종료
	EndAbility(true);
}
