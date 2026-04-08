// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/ConsumptionItem.h"
#include "Items/GrenadeProjectile.h"
#include "GrenadeItem.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API AGrenadeItem : public AConsumptionItem
{
	GENERATED_BODY()

public:
	AGrenadeItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade Data")
	TSubclassOf<AGrenadeProjectile> ProjectileClassToSpawn;
};
