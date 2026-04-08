// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/CustomAbility.h"
#include "CustomASC.h"
#include "GameFramework/Character.h"


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

		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Owner Check!"));
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

/////////////////////////////////////////////////////////////////////////////
// Ability Private 함수

bool UCustomAbility::CanExecute() const
{
	if (!OwnerASC) {
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Execute Failed!"));
		return false;
	}

	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Execute!"));

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
// 이벤트(Payload 데이터 포함)를 통해 어빌리티 실행 시도
bool UCustomAbility::TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
	// 실행 조건 체크
	if (!CanExecute()) return false;

	// 실행 중 태그 부여
	if (OwnerASC)
	{
		OwnerASC->AddGameplayTags(ActivationOwnedTags);
	}

	// 자원 소모 및 쿨타임 적용 (구현해두신 Commit)
	CommitAbility();

	// ★ 핵심: 일반 ActivateAbility() 대신, 데이터를 넘겨주는 함수를 호출합니다!
	ActivateAbilityWithEvent(Payload);

	return true;
}

// 자식 클래스에서 덮어쓸 가상 함수 (기본적으로는 일반 ActivateAbility를 부르도록 함)
