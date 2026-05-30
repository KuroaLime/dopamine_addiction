// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "TPSPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;

/**
 * 
 */
UCLASS(Abstract)
class MANAGER_API ATPSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendAction(FName ActionName);

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> TPSMappingContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputMappingContext* TPSMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = UI)
	TSubclassOf<class UTpsPlayerMainHUD> PlayerTPSUI;

	UPROPERTY(Transient)
	UTpsPlayerMainHUD* PlayerHUDWidget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Look;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Aim;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_StartFire();
	void Input_StopFire();
	void Input_Aim();
	void Input_AimEnd();

	virtual void TPS_UI();
};
