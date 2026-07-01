// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/CustomAbility.h"
#include "Default/Ability/CustomASC.h"
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
UCustomAbility::UCustomAbility()
{
	OwnerASC = nullptr;
	AvatarActor = nullptr;
	OwnerCharacter = nullptr;
}

void UCustomAbility::InitializeAbility(UCustomASC* InASC)
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

UWorld* UCustomAbility::GetWorld() const
{
	return (AvatarActor ? AvatarActor->GetWorld() : nullptr);
}

// 시도 성공 여부 반환
bool UCustomAbility::TryActivateAbility()
{
	if (!CanExecute()) return false;
	if (OwnerASC)
	{
		OwnerASC->AddGameplayTags(ActivationOwnedTags);
	}
	CommitAbility();
	ActivateAbility();

	return true;
}

// 강제 취소
void UCustomAbility::CancelAbility()
{
	EndAbility(true);
}

const FGameplayTagContainer& UCustomAbility::GetAbilityTags() const
{
	return AbilityTags;
}

void UCustomAbility::ClearInterfaceCache()
{
	CachedPSInterface = nullptr;
	CachedGSInterface = nullptr;
	CachedOwnerInterface = nullptr;
	CachedPCInterface = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
// Ability Private 함수

bool UCustomAbility::CanExecute() const
{
	if (!OwnerASC) {
		return false;
	}
	return true;
}

void UCustomAbility::ActivateAbility()
{
	if (!CanExecute()) return;
}

void UCustomAbility::CommitAbility()
{
	
}

void UCustomAbility::EndAbility(bool bWasCancelled)
{
	if (OwnerASC)
	{
		OwnerASC->RemoveGameplayTags(ActivationOwnedTags);
	}
}

void UCustomAbility::ActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
	ActivateAbility();
}

bool UCustomAbility::TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
	if (!CanExecute()) return false;

	if (OwnerASC)
	{
		OwnerASC->AddGameplayTags(ActivationOwnedTags);
	}

	CommitAbility();

	ActivateAbilityWithEvent(Payload);

	return true;
}

// 자식 클래스에서 덮어쓸 가상 함수 (기본적으로는 일반 ActivateAbility를 부르도록 함)

/////////////////////////////////////////////////////////////////////////////
// 인터페이스 캐싱
IPhasePlayerStateInterface* UCustomAbility::GetPSInterface()
{
	if (CachedPSInterface) return CachedPSInterface;

	if (OwnerCharacter && OwnerCharacter->GetPlayerState())
	{
		CachedPSInterface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	}
	return CachedPSInterface;
}

IPhaseGameStateInterface* UCustomAbility::GetGSInterface()
{
	if (CachedGSInterface) return CachedGSInterface;

	if (GetWorld() && GetWorld()->GetGameState())
	{
		CachedGSInterface = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState());
	}
	return CachedGSInterface;
}

IAbilityOwnerInterface* UCustomAbility::GetOwnerInterface()
{
	if (CachedOwnerInterface) return CachedOwnerInterface;

	if (OwnerCharacter)
	{
		CachedOwnerInterface = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	}
	return CachedOwnerInterface;
}

IPhasePlayerControllerInterface* UCustomAbility::GetPCInterface()
{
	if (CachedPCInterface) return CachedPCInterface;

	if (OwnerCharacter && OwnerCharacter->GetController())
	{
		CachedPCInterface = Cast<IPhasePlayerControllerInterface>(OwnerCharacter->GetController());
	}

	return CachedPCInterface;
}