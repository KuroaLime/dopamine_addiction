// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MANAGER_API IInteractableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnBeginFocus();

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnEndFocus();

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void Interact(APawn* Interactor);
};
