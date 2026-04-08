// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityDrop.h"
#include "ManagerCharacter.h"
#include "Items/BaseItem.h"

UAbilityDrop::UAbilityDrop()
{
	// Q키를 누르면 이 태그를 찾아서 실행
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Drop")));
}

void UAbilityDrop::ActivateAbility()
{
	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);
	if (!Character)
	{
		EndAbility(true);
		return;
	}

	// [변경 1] 인벤토리가 비어있는지 확인
	// (기존: Character->CurrentItem == nullptr)
	if (Character->Inventory.Num() == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Inventory is Empty. Nothing to drop."));
		EndAbility(true);
		return;
	}

	// 2. 몽타주 재생 및 타이머 (여기는 완벽합니다! 그대로 유지)
	if (MontageToPlay)
	{
		float Duration = Character->PlayAnimMontage(MontageToPlay);

		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			this,
			&UAbilityDrop::RealDrop,
			Duration,
			false
		);

		return;
	}
	else
	{
		RealDrop();
	}
}

void UAbilityDrop::RealDrop()
{
	// --- 여기가 진짜 버리기 로직 ---
	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);

	if (Character)
	{
		Character->DropLastItem();
	}

	// 4. 종료
	EndAbility(true);
}