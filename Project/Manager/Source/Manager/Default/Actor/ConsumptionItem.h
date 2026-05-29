// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Actor/UsableItem.h"
#include "ConsumptionItem.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API AConsumptionItem : public AUsableItem
{
	GENERATED_BODY()

public:
	AConsumptionItem();

	virtual void OnPickedUp(class AManagerCharacter* Player) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	int32 MaxStackCount;
};
