// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "InputActionValue.h"
#include "TPSInputHandler.generated.h"

class UInputAction;
class AMainPlayerController;

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UTPSInputHandler : public UInputHandler
{
	GENERATED_BODY()

public:
	UTPSInputHandler();

	virtual void BeginPlay() override;

public:
	virtual void SetupInput(UEnhancedInputComponent* EnhancedInputComponent) override;

	virtual void InputActivate() override;
	virtual void InputDeactivate() override;

protected:
	virtual void AddInputMappingContexts() override;
	virtual void RemoveInputMappingContexts() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Jump = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Move = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Look = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	class UInputAction* IA_Crouch = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Fire = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Aim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Shop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_PickupCard = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Reload = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_CardDiscard = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Quit = nullptr;

protected:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_Crouch();
	void Input_CrouchEnd();
	void Input_StartFire();
	void Input_StopFire();
	void Input_Aim();
	void Input_AimEnd();
	void Input_Shop();
	void Input_PickupCard();
	void Input_Reload();
	void Input_CardDiscard();
	void Input_CardDiscardEnd();
	void Input_Quit();

	bool bOpenShop = false;
};
