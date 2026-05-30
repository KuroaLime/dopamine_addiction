// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "TPSCharacter.generated.h"

// =============================================================================
// [전방 선언]
// =============================================================================
class USpringArmComponent;
class UCameraComponent;
class UWidgetComponent;
class UInteractionComponent;
class UCharacterStateComponent;
class UCameraStateComponent;
class AWeapon;
class UCustomASC;
class UCustomAbility;
struct FInputActionValue;

UCLASS(Abstract)
class MANAGER_API ATPSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATPSCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(VisibleAnywhere, Category = State)
	UCharacterStateComponent* CharacterState;

	UPROPERTY(VisibleAnywhere, Category = State)
	UCameraStateComponent* CameraState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UCustomASC* AbilitySystemComponent;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE AWeapon* GetEquippedGun() const { return m_pEquippedGun; }
	FORCEINLINE UCustomASC* GetCustomASC() const { return AbilitySystemComponent; }

	UCameraComponent* SetFollowCamera() { return FollowCamera; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWeapon> m_cGun;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	AWeapon* m_pEquippedGun;

	UPROPERTY(EditAnywhere, Category = "GAS")
	TArray<TSubclassOf<UCustomAbility>> DefaultAbilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractionComponent* InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HPBarWidget;
};
