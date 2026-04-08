// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/CustomAbility.h"
#include "GA_Interaction.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UGA_Interaction : public UCustomAbility
{
	GENERATED_BODY()
public:
	UGA_Interaction();

protected:
	virtual void ActivateAbility() override;
};
