// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Aim.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Data/CameraStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UAbility_Aim::UAbility_Aim()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming")));
}

void UAbility_Aim::LocalActivateWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	if (UCameraStateComponent* CamState = Owner->GetCameraStateComponent())
	{
		CamState->SmoothZoom(true);
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UAbility_Aim::LocalCancelWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	if (UCameraStateComponent* CamState = Owner->GetCameraStateComponent())
	{
		CamState->SmoothZoom(false);
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = true;
	Character->bUseControllerRotationYaw = false;
}


void UAbility_Aim::ActivateAbility()
{
	// 서버: ActivationOwnedTags("State.Movement.Aiming")는
	// TryActivateAbility -> AddGameplayTags에서 자동 부여됨.
	// 지속형이므로 EndAbilityNow를 호출하지 않는다.
	// 에임 해제는 CancelAbilitiesWithTag -> CancelAbility -> EndAbility 경로로 온다.

	if (OwnerCharacter)
	{
		OwnerCharacter->bUseControllerRotationYaw = true;
		OwnerCharacter->GetCharacterMovement()->bOrientRotationToMovement = false;
	}
}

void UAbility_Aim::EndAbility(bool bWasCancelled)
{
	// 부모: bIsActive = false + ActivationOwnedTags("State.Movement.Aiming") 제거
	Super::EndAbility(bWasCancelled);

	// 로컬 카메라/회전 복구
	// EndAbility는 서버에서 호출되지만, OwnerCharacter는 서버 인스턴스.
	// 클라이언트의 카메라/회전 복구는 OnOwnedTagsChanged 델리게이트를 통해
	// "State.Movement.Aiming" 태그가 제거됐을 때 Character/Controller 쪽에서 처리하는 것이 맞다.
	// 여기서는 서버의 OwnerCharacter 상태만 복구한다.
}

void UAbility_Aim::RestoreLocalState(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	if (UCameraStateComponent* CamState = Owner->GetCameraStateComponent())
	{
		CamState->SmoothZoom(false);
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = true;
	Character->bUseControllerRotationYaw = false;
}