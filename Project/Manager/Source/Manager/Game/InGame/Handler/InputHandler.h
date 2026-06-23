// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "Default/Ability/Interface/AbilitySystemInterface.h"
#include "InputHandler.generated.h"

class APlayerController;

UCLASS( Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UInputHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInputHandler();

	virtual void BeginPlay() override;

public:
	virtual void SetupInput(UEnhancedInputComponent* EnhancedInputComponent) {}

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void InputActivate();

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void InputDeactivate();

protected:
	UPROPERTY()
	APlayerController* OwnerController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MappingContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* MappingContext;

protected:
	virtual void AddInputMappingContexts() {}
	virtual void RemoveInputMappingContexts() {}

	void AddMappingContext(UInputMappingContext* Context, int32 Priority = 0);
	void RemoveMappingContext(UInputMappingContext* Context);
	void ClearAllMappingContexts();

	UPFGASC* ResolveOwnerASC() const;
};
