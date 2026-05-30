// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AbilityOwnerInterface.generated.h"

class UCameraStateComponent;
class UCameraComponent;
class AActor;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAbilityOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MANAGER_API IAbilityOwnerInterface
{
	GENERATED_BODY()

public:
	virtual UCameraStateComponent* GetCameraStateComponent() const = 0;
	virtual UCameraComponent* GetFollowCameraComponent() const = 0;

	virtual AActor* GetEquippedWeapon() const = 0;
};
