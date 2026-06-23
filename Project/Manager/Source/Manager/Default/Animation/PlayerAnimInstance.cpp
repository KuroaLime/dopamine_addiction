// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Animation/PlayerAnimInstance.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/InGame/MainPlayerState.h"
#include "Default/Ability/Interface/AbilityCheckInterface.h"
#include "Kismet/KismetMathLibrary.h"
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

	if (IAbilityCheckInterface* DeathInterface = Cast<IAbilityCheckInterface>(OwnerCharacter))
	{
		bIsDeath = DeathInterface->IsCharacterDeath();
	}
	else
	{
		bIsDeath = false;
	}
	AMainPlayerState* PS = OwnerNow->GetPlayerState<AMainPlayerState>();
	if (PS)
	{
		CurrentWeaponType = PS->GetWeaponID();
	}
	else
	{
		CurrentWeaponType = EWeaponType::None;
	}
	FRotator ActorRotation = OwnerCharacter->GetActorRotation();

	FRotator ControlRotation = OwnerCharacter->GetBaseAimRotation();

	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(ControlRotation, ActorRotation);

	AimYaw = DeltaRot.Yaw;
	AimPitch = DeltaRot.Pitch;
}

void UPlayerAnimInstance::PlayFireMontage(EWeaponType WeaponType)
{
	if (WeaponFireMontages.Contains(WeaponType)) {
		if (UAnimMontage* TargetMontage = WeaponFireMontages[WeaponType]) {
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(1, 1.1f, FColor::Yellow, TEXT("WeaponActorsss"));
			Montage_Play(TargetMontage);
		}
	}
}

void UPlayerAnimInstance::PlayReloadMontage(EWeaponType WeaponType)
{
	if (WeaponReloadMontages.Contains(WeaponType)) {
		if (UAnimMontage* TargetMontage = WeaponReloadMontages[WeaponType]) {
			Montage_Play(TargetMontage);
		}
	}
}
