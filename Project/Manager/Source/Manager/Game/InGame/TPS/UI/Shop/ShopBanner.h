// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "ShopBanner.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UShopBanner : public UWidgetParent
{
	GENERATED_BODY()
protected:

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Rand_Upgrade_TEXT;

    UPROPERTY(meta = (BindWidget))
    class UImage* Rand_Upgrade_Background;
    UPROPERTY(meta = (BindWidget))
    class UImage* Background;
public:
    void UpdateUpgradeInfo(const FString& Name, UTexture2D* Icon);
};
