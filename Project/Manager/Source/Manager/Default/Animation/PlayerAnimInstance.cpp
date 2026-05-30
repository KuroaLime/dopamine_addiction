// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Animation/PlayerAnimInstance.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Game/InGame/TPS/System/TPSCharacter.h"
#include "Default/Ability/CustomASC.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (APawn* PlayerPawn = TryGetPawnOwner()) {
		TPSCharacter = Cast<ATPSCharacter>(PlayerPawn);
		MoveComp = TPSCharacter ? TPSCharacter->GetCharacterMovement() : nullptr;
	}
}

void UPlayerAnimInstance::UpdateAnimProperties(float DeltaTime)
{
	APawn* OwnerNow = TryGetPawnOwner();
	if (OwnerNow != TPSCharacter || !IsValid(MoveComp)) {
		TPSCharacter = Cast<ATPSCharacter>(OwnerNow);
		MoveComp = TPSCharacter ? TPSCharacter->GetCharacterMovement() : nullptr;
	}

	if (!TPSCharacter || !MoveComp) return;
	const FVector Vel = TPSCharacter->GetVelocity();
	Speed = Vel.Size2D();

	bIsInAir = MoveComp->IsFalling();


	bIsAccelerating = MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;

	//Direction = CalculateDirection(Vel, TPSCharacter->GetActorRotation());
	Direction = UKismetAnimationLibrary::CalculateDirection(Vel, TPSCharacter->GetActorRotation());
	bIsMoving = (Speed > 3.0f) && bIsAccelerating;
	
	if (TPSCharacter && TPSCharacter->AbilitySystemComponent)
	{
		// 현재 ASC에 "State.Movement.Aiming" 태그가 하나라도 있는지 확인
		FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming"));
		bIsAiming = TPSCharacter->AbilitySystemComponent->HasAnyMatchingGameplayTags(FGameplayTagContainer(AimTag));
	}
}
