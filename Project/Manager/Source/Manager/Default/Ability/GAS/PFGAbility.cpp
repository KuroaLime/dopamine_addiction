// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/GAS/PFGAbility.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/GAS/PFGAttributeSet.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"

#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

/////////////////////////////////////////////////////////////////////////////
// Ability 초기화
UPFGAbility::UPFGAbility()
{
	OwnerASC = nullptr;
	AvatarActor = nullptr;
	OwnerCharacter = nullptr;
	CooldownDuration = 0.f;
	CostAmount = 0.f;
}

void UPFGAbility::InitializeAbility(UPFGASC* InASC)
{
	OwnerASC = InASC;

	if (OwnerASC)
	{
		AvatarActor = OwnerASC->GetOwner();
		OwnerCharacter = Cast<ACharacter>(AvatarActor);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Ability Public 함수

UWorld* UPFGAbility::GetWorld() const
{
	return (AvatarActor ? AvatarActor->GetWorld() : nullptr);
}

bool UPFGAbility::TryActivateAbility()
{
	if (!CanExecute()) return false;

	bIsActive = true;

	if (OwnerASC)
	{
		OwnerASC->AddGameplayTags(ActivationOwnedTags);
	}
	CommitAbility();
	ActivateAbility();

	return true;
}

void UPFGAbility::CancelAbility()
{
	if (!bIsActive)
	{
		return;
	}

	EndAbility(true);
}

const FGameplayTagContainer& UPFGAbility::GetAbilityTags() const
{
	return AbilityTags;
}

const FGameplayTagContainer& UPFGAbility::GetTriggerTags() const
{
	return TriggerTags;
}

const FGameplayTagContainer& UPFGAbility::GetCooldownTags() const
{
	return CooldownTags;
}

const FGameplayTagContainer& UPFGAbility::GetActivationBlockedTags() const
{
	return ActivationBlockedTags;
}

const FGameplayTagContainer& UPFGAbility::GetActivationRequiredTags() const
{
	return ActivationRequiredTags;
}

const FGameplayTagContainer& UPFGAbility::GetBlockAbilitiesWithTag() const
{
	return BlockAbilitiesWithTag;
}

const FGameplayTagContainer& UPFGAbility::GetCancelAbilitiesWithTag() const
{
	return CancelAbilitiesWithTag;
}

float UPFGAbility::GetCooldownDuration() const
{
	return CooldownDuration;
}

float UPFGAbility::GetCostAmount() const
{
	return CostAmount;
}

void UPFGAbility::ClearInterfaceCache()
{
	CachedPSInterface = nullptr;
	CachedGSInterface = nullptr;
	CachedOwnerInterface = nullptr;
	CachedPCInterface = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
// Ability Private 함수

bool UPFGAbility::CanExecute() const
{
	if (!OwnerASC)
	{
		return false;
	}

	// 이미 실행 중이면 재진입 차단 (ActivationOwnedTags 중첩 부여 방지)
	if (bIsActive)
	{
		return false;
	}

	// ActivationBlockedTags: 하나라도 보유 중이면 실행 불가 (예: 스턴, 침묵)
	if (!ActivationBlockedTags.IsEmpty() &&
		OwnerASC->HasAnyMatchingGameplayTags(ActivationBlockedTags))
	{
		return false;
	}

	// ActivationRequiredTags: 전부 보유해야 실행 가능
	if (!ActivationRequiredTags.IsEmpty())
	{
		for (const FGameplayTag& RequiredTag : ActivationRequiredTags)
		{
			if (!OwnerASC->HasMatchingGameplayTag(RequiredTag))
			{
				return false;
			}
		}
	}

	return true;
}

void UPFGAbility::ActivateAbility()
{
	// 주의: 여기서 CanExecute()를 다시 호출하면 안 된다.
	// TryActivateAbility()가 이미 CanExecute() 통과를 확인한 뒤 bIsActive = true로
	// 설정하고 이 함수를 호출하므로, CanExecute()를 재호출하면 "이미 실행 중"
	// 재진입 방지 검사에 걸려 항상 false가 된다.
	// 그 결과 EndAbilityNow()가 호출되지 않아 bIsActive가 true로 영구히 남고,
	// ActivationOwnedTags도 절대 제거되지 않는 버그가 발생한다.
	//
	// 기본 동작: 즉시 종료형 어빌리티로 간주하고 바로 EndAbility를 호출해
	// ActivationOwnedTags가 영구히 남지 않게 한다.
	// 지속형(채널링/버프/타이머 기반) 어빌리티를 만들 때는 이 함수를 오버라이드해
	// Super::ActivateAbility()를 호출하지 않고, 효과가 끝나는 시점에 직접
	// EndAbilityNow()를 호출해야 한다.
	EndAbilityNow();
}

void UPFGAbility::CommitAbility()
{
	if (!OwnerASC) return;

	// 다른 어빌리티 강제 취소
	if (!CancelAbilitiesWithTag.IsEmpty())
	{
		OwnerASC->CancelAbilitiesWithTag(CancelAbilitiesWithTag);
	}
}

void UPFGAbility::EndAbility(bool bWasCancelled)
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;

	if (OwnerASC)
	{
		OwnerASC->RemoveGameplayTags(ActivationOwnedTags);
	}
}

void UPFGAbility::EndAbilityNow()
{
	if (!bIsActive) return; // 이미 종료됨 (중복 호출 방지)
	EndAbility(false);
}

void UPFGAbility::ActivateAbilityWithEvent(const FPFGGameplayEventData& Payload)
{
	ActivateAbility();
}

bool UPFGAbility::TryActivateAbilityWithEvent(const FPFGGameplayEventData& Payload)
{
	if (!CanExecute()) return false;

	bIsActive = true;

	if (OwnerASC)
	{
		OwnerASC->AddGameplayTags(ActivationOwnedTags);
	}

	CommitAbility();

	ActivateAbilityWithEvent(Payload);

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// 인터페이스 캐시
IPhasePlayerStateInterface* UPFGAbility::GetPSInterface()
{
	if (CachedPSInterface) return CachedPSInterface;

	if (OwnerCharacter && OwnerCharacter->GetPlayerState())
	{
		CachedPSInterface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	}
	return CachedPSInterface;
}

IPhaseGameStateInterface* UPFGAbility::GetGSInterface()
{
	if (CachedGSInterface) return CachedGSInterface;

	if (GetWorld() && GetWorld()->GetGameState())
	{
		CachedGSInterface = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState());
	}
	return CachedGSInterface;
}

IAbilityOwnerInterface* UPFGAbility::GetOwnerInterface()
{
	if (CachedOwnerInterface) return CachedOwnerInterface;

	if (OwnerCharacter)
	{
		CachedOwnerInterface = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	}
	return CachedOwnerInterface;
}

IPhasePlayerControllerInterface* UPFGAbility::GetPCInterface()
{
	if (CachedPCInterface) return CachedPCInterface;

	if (OwnerCharacter && OwnerCharacter->GetController())
	{
		CachedPCInterface = Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());
	}

	return CachedPCInterface;
}
