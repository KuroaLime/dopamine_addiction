// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Default/Ability/Interface/AbilityCheckInterface.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Ability/Interface/AbilitySystemInterface.h"
#include "Game/InGame/Interface/PhaseCharacterInterface.h"
#include "MainCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UWidgetComponent;
class UInteractionComponent;
class UCharacterStateComponent;
class UCameraStateComponent;
class AWeapon;
class UPFGASC;
class UPFGAbility;
class UAIPerceptionStimuliSourceComponent;
struct FInputActionValue;

UCLASS()
class MANAGER_API AMainCharacter : public ACharacter,
								   public IAbilityCheckInterface,
								   public IAbilityOwnerInterface,
								   public IAbilitySystemInterface,
								   public IPhaseCharacterInterface
{
	GENERATED_BODY()

public:
	AMainCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;

public:
	virtual bool IsCharacterAiming() const override;
	virtual bool IsCharacterDeath() const override;
	virtual UCameraStateComponent* GetCameraStateComponent() const override { return CameraState; }
	virtual UCameraComponent* GetFollowCameraComponent() const override { return FollowCamera; }
	virtual AActor* GetEquippedWeapon() const override { return m_pEquippedGun; }
	virtual UPFGASC* GetASC() const override { return AbilitySystemComponent; }
	virtual UCharacterStateComponent* GetCharacterState() const override { return CharacterState; }
	virtual void EquipWeapon(EWeaponType NewWeaponID) override;
	virtual void SetSitting(bool bNewSitting) override;
	bool IsSitting() const { return bIsSitting; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	void InitPlayerData();

private:
	UPROPERTY()
	class UHealthRegenComponent* HealthRegen;
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
	UPFGASC* AbilitySystemComponent;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION()
	void OnCharacterDeath();

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE AWeapon* GetEquippedGun() const { return m_pEquippedGun; }

	UCameraComponent* SetFollowCamera() { return FollowCamera; }
protected:
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWeapon> m_cGun;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TMap<EWeaponType, TSubclassOf<AWeapon>> WeaponClasses;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedGun, VisibleAnywhere, Category = "Weapon")
	AWeapon* m_pEquippedGun;
	UFUNCTION()
	void OnRep_EquippedGun();

	// 카드 라운드 좌석 착석 여부. MainAnimInstance가 매 프레임 읽어서 앉기 포즈(MM_Sitting_Idle)로 전환한다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsSitting = false;

	UPROPERTY(EditAnywhere, Category = "GAS")
	TArray<TSubclassOf<UPFGAbility>> DefaultAbilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	UInteractionComponent* InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HPBarWidget;

	// 몬스터 AI(황금 고블린 등)의 Sight Perception이 플레이어를 감지할 수 있게 하는 자극원.
	// 이게 없으면 AIPerceptionComponent가 아무리 Sight를 설정해도 플레이어를 "볼" 수 없다.
	UPROPERTY(VisibleAnywhere, Category = "AI")
	UAIPerceptionStimuliSourceComponent* AIStimuliSource;
public:
	void UpdateCharacterStats();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	void OnPlayerDataChanged(const FPlayerData& NewData);
	void OnAccumulatedUpgradesChanged(const FAccumulatedUpgrades& NewUpgrades);
	bool bWasAiming;
};
