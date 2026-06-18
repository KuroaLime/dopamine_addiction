// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "InputActionValue.h"
#include "CardInputHandler.generated.h"

class UInputAction;

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UCardInputHandler : public UInputHandler
{
	GENERATED_BODY()
	
public:
	UCardInputHandler();

	virtual void BeginPlay() override;

public:
	virtual void SetupInput(UEnhancedInputComponent* EnhancedInputComponent) override;
	virtual void InputActivate() override;
	virtual void InputDeactivate() override;

protected:
	virtual void AddInputMappingContexts() override;
	virtual void RemoveInputMappingContexts() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_Check = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_Call = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_Half = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_Die = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_AllIn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_SelectCard1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_SelectCard2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_SelectCard3 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Card")
	UInputAction* IA_ConfirmSelection = nullptr;

protected:
	bool bSelectedCard0 = false;
	bool bSelectedCard1 = false;
	bool bSelectedCard2 = false;

protected:
	void Input_Check();
	void Input_Call();
	void Input_Half();
	void Input_Die();
	void Input_AllIn();
	void Input_SelectCard1();
	void Input_SelectCard2();
	void Input_SelectCard3();
	void Input_ConfirmSelection();
};
