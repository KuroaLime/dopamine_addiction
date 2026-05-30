// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Default/Ability/CustomASC.h"
#include "AbilitySystemInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAbilitySystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MANAGER_API IAbilitySystemInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual UCustomASC* GetCustomASC() const = 0;
};
