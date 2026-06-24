// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

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

	UPROPERTY(meta = (BindWidget))
	class UImage* Static_Upgrade_Background = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UImage* Background = nullptr;
	
	UPROPERTY(meta = (BindWidget))
	class UShopBanner* BP_ShopBanner;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* StaticUpgradeText;

protected:
	UFUNCTION()
	void HandleUpgradePurchase(int32 ItemID);
	UFUNCTION()
	void HandleUpgradCharacterState(int32 ItemID);
	UFUNCTION()
	void bRandomCardSelected(int32 CardID);
	UFUNCTION()
	void ReturnToShopButtons();

	UFUNCTION()
	void SendToSelectionCardID(int32 CardID);
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;
protected:
	virtual void NativeConstruct() override;

public:
	void Update_UpgradeSelectionWidget(const TArray<FRandomCardOption>& Options);
	
};