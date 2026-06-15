// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Animation/PlayerAnimInstance.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Default/Ability/Interface/AbilityCheckInterface.h"

void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (APawn* PlayerPawn = TryGetPawnOwner()) {
		OwnerCharacter = Cast<ACharacter>(PlayerPawn);
		MoveComp = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	}
}

void UPlayerAnimInstance::UpdateAnimProperties(float DeltaTime)
{
	APawn* OwnerNow = TryGetPawnOwner();
	if (!OwnerNow) return;

	if (OwnerNow != OwnerCharacter || !IsValid(MoveComp)) {
		OwnerCharacter = Cast<ACharacter>(OwnerNow);
		MoveComp = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	}

	if (!OwnerCharacter || !MoveComp) return;

	const FVector Vel = OwnerCharacter->GetVelocity();
	Speed = Vel.Size2D();

	bIsInAir = MoveComp->IsFalling();

	bIsAccelerating = MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;

	Direction = UKismetAnimationLibrary::CalculateDirection(Vel, OwnerCharacter->GetActorRotation());
	bIsMoving = (Speed > 3.0f) && bIsAccelerating;

	if (IAbilityCheckInterface* AimInterface = Cast<IAbilityCheckInterface>(OwnerCharacter))
	{
		bIsAiming = AimInterface->IsCharacterAiming();

	}
	else
	{
		bIsAiming = false;
	}
}
