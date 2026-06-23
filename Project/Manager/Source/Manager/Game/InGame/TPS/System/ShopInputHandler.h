// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "ShopInputHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UShopInputHandler : public UInputHandler
{
	GENERATED_BODY()
	
public:
	UShopInputHandler();

	virtual void BeginPlay() override;

public:
	virtual void SetupInput(UEnhancedInputComponent* EnhancedInputComponent) override;

	virtual void InputActivate() override;
	virtual void InputDeactivate() override;

protected:
	virtual void AddInputMappingContexts() override;
	virtual void RemoveInputMappingContexts() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Shop")
	UInputAction* IA_Out = nullptr;

protected:
	void Input_Out();
};
