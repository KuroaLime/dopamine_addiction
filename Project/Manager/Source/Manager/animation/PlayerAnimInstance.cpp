// Fill out your copyright notice in the Description page of Project Settings.


#include "animation/PlayerAnimInstance.h"
//#include "PlayerAnimInstance.h"
#include "ManagerCharacter.h"
#include "CustomASC.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (APawn* PlayerPawn = TryGetPawnOwner()) {
		ManagerCharacter = Cast<AManagerCharacter>(PlayerPawn);
		MoveComp = ManagerCharacter ? ManagerCharacter->GetCharacterMovement() : nullptr;
	}
}

void UPlayerAnimInstance::UpdateAnimProperties(float DeltaTime)
{
	APawn* OwnerNow = TryGetPawnOwner();
	if (OwnerNow != ManagerCharacter || !IsValid(MoveComp)) {
		ManagerCharacter = Cast<AManagerCharacter>(OwnerNow);
		MoveComp = ManagerCharacter ? ManagerCharacter->GetCharacterMovement() : nullptr;
	}

	if (!ManagerCharacter || !MoveComp) return;
	const FVector Vel = ManagerCharacter->GetVelocity();
	Speed = Vel.Size2D();

	bIsInAir = MoveComp->IsFalling();


	bIsAccelerating = MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;

	//Direction = CalculateDirection(Vel, ManagerCharacter->GetActorRotation());
	Direction = UKismetAnimationLibrary::CalculateDirection(Vel, ManagerCharacter->GetActorRotation());
	bIsMoving = (Speed > 3.0f) && bIsAccelerating;
	
	if (ManagerCharacter && ManagerCharacter->AbilitySystemComponent)
	{
		// 현재 ASC에 "State.Movement.Aiming" 태그가 하나라도 있는지 확인
		FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming"));
		bIsAiming = ManagerCharacter->AbilitySystemComponent->HasAnyMatchingGameplayTags(FGameplayTagContainer(AimTag));
	}
}
