// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "PlayerAnimInstance.generated.h"
class UCharacterMovementComponent;
/**
 * 
 */
UCLASS()
class MANAGER_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
	virtual void NativeInitializeAnimation() override;

	UFUNCTION(BlueprintCallable)
	void UpdateAnimProperties(float DeltaTime);

private:
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MoveComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsInAir = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsMoving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	float Speed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsDeath = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	EWeaponType CurrentWeaponType = EWeaponType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	float AimYaw;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	float AimPitch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Player, meta = (AllowPrivateAccess = "true"))
	bool bIsCrouch = false;

public:
	UFUNCTION(BlueprintCallable,Category="Animation Montage")
	void PlayFireMontage(EWeaponType WeaponType);
	UFUNCTION(BlueprintCallable, Category = "Animation Montage")
	void PlayReloadMontage(EWeaponType WeaponType);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TMap<EWeaponType, UAnimMontage*> WeaponFireMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TMap<EWeaponType, UAnimMontage*> WeaponReloadMontages;
};
