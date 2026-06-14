
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Default/Ability/Interface/AbilityCheckInterface.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Ability/Interface/AbilitySystemInterface.h"
#include "ManagerCharacter.generated.h"

class UPFGASC;
class UPFGAbility;

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AManagerCharacter : public ACharacter,
	public IAbilityCheckInterface,
	public IAbilityOwnerInterface,
	public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWeapon> m_cGun; // class AActor에서 AWeapon으로 변경

	// 2. 실제 스폰된 무기를 담을 변수를 추가합니다.
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	AWeapon* m_pEquippedGun;
public:
	AManagerCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

public:
	virtual bool IsCharacterAiming() const override;
	virtual UCameraStateComponent* GetCameraStateComponent() const override { return CameraState; }
	virtual UCameraComponent* GetFollowCameraComponent() const override { return FollowCamera; }
	virtual AActor* GetEquippedWeapon() const override { return m_pEquippedGun; }
	virtual UPFGASC* GetASC() const override { return AbilitySystemComponent; }
	virtual UCharacterStateComponent* GetCharacterState() const override { return CharacterState; }

public:
	//캐릭터 상태 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = State)
	class UCharacterStateComponent* CharacterState;
	//카메라 조절 기능 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = State)
	class UCameraStateComponent* CameraState;

public:
	//UCustomASC에서 관리하도록 옮기자
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UPFGASC* AbilitySystemComponent;
	UPROPERTY(EditAnywhere, Category = "GAS")
	TArray<TSubclassOf<UPFGAbility>> DefaultAbilities;

protected:
	//필요한가?
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

public:
	void* SetFollowCamera() { return FollowCamera; }
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	AWeapon* GetEquippedGun() const { return m_pEquippedGun; }

public:
	float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser);

public:
	UPROPERTY(VisibleAnywhere, Category = UI)
	class UWidgetComponent* HPBarWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	class UInteractionComponent* InteractionComponent;
	
};

