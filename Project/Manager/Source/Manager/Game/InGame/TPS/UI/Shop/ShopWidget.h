// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

#include "ShopWidget.generated.h"
/**
 *
 */

#define ShopLvTotalNumber 8

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

	UPROPERTY(meta = (BindWidget))
	class UWidget* StatLevelPanel = nullptr;

	// 0: Health, 1: HealthRegen, 2: MoveSpeed, 3: WeaponDamage, 4: FireRate, 5: Range, 6: Magazine, 7: Reload
	UPROPERTY()
	class UImage* Lv_Image[ShopLvTotalNumber] = {};

	UPROPERTY()
	class UMaterialInstanceDynamic* LvLinearMID[ShopLvTotalNumber] = {};

	static const FName LvLinearWipeParamName;

	UPROPERTY(meta = (BindWidget))
	class UShopButton* Rand_UpgradeBTN00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UShopButton* Rand_UpgradeBTN01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UShopButton* Rand_UpgradeBTN02 = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UImage* Rand_UpgradeIMG00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UImage* Rand_UpgradeIMG01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UImage* Rand_UpgradeIMG02 = nullptr;

	// 총기 개조 상품 슬롯(0~2)에 표시할 무기 스탯별 이름/아이콘. 아이콘은 아직 준비 안 됐으면 비워둬도 됨.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop | WeaponUpgrade", meta = (AllowPrivateAccess = "true"))
	TMap<EUpgradeType, FText> WeaponUpgradeNameMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop | WeaponUpgrade", meta = (AllowPrivateAccess = "true"))
	TMap<EUpgradeType, class UTexture2D*> WeaponUpgradeIconMap;

	UFUNCTION()
	void HandleWeaponUpgradePurchase(int32 SlotIndex);
	void UpdateWeaponUpgradeButtons();

	void TryBindGameStateDelegate();
	bool bBoundGameStateDelegate = false;

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
	void UpdateUpgradeButtons();

private:
	FTimerHandle BindingTimerHandle;
	bool bBoundDelegates = false;
	void TryBindPlayerStateDelegates();
	void OnPlayerDataChanged(const FPlayerData& NewPlayerData);
	void OnGoldChanged(float NewGold);
	void OnAccumulatedUpgradesChanged(const struct FAccumulatedUpgrades& NewUpgrades);
	void UpdateStatLevelWidgets();
};