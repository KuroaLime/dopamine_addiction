// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "ShopWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UShopWidget : public UWidgetParent
{
	GENERATED_BODY()
private:
	UPROPERTY(meta=(BindWidget))
	class UShopButton* UpgradeButton00 = nullptr;


	UPROPERTY()
	TArray<class UShopButton*> Char_UpgradeButtons;


	UPROPERTY(meta = (BindWidget))
	class UUpgradeSelectionWidget* CardSelectionPanel = nullptr;

protected:
	UFUNCTION()
	void HandleUpgradePurchase(int32 ItemID);
	UFUNCTION()
	void HandleUpgradCharacterState(int32 ItemID);
	UFUNCTION()
	void ReturnToShopButtons();
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;
protected:
	virtual void NativeConstruct() override;

public:
	void Update_UpgradeSelectionWidget();
	
};