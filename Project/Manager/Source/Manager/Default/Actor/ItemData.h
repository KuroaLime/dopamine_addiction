// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemData.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Consumalbe,
	Storable,
	Skill,
	Equipment
};


UCLASS()
class MANAGER_API UItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere) EItemType ItemType;
	UPROPERTY(EditAnywhere) FText ItemName;
	UPROPERTY(EditAnywhere) UTexture2D* Icon;

	//UPROPERTY(EditAnywhere) TSubclassOf<class UGameplayEffect> ItemEffect;
};
