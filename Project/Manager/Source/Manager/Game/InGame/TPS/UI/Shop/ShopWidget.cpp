// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"
#include "Manager.h"
#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/InGame/TPS/UI/Shop/UpgradeSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/TPS/UI/Shop/ShopBanner.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainGameState.h"
#include "Materials/MaterialInstanceDynamic.h"

const FName UShopWidget::LvLinearWipeParamName(TEXT("Linear_wipe"));

namespace
{
	FText GetDefaultWeaponUpgradeName(EUpgradeType Type)
	{
		switch (Type)
		{
		case EUpgradeType::Weapon_Damage:   return FText::FromString(TEXT("공격력 개조"));
		case EUpgradeType::Weapon_FireRate: return FText::FromString(TEXT("연사력 개조"));
		case EUpgradeType::Weapon_Range:    return FText::FromString(TEXT("사거리 개조"));
		case EUpgradeType::Weapon_Magazine: return FText::FromString(TEXT("탄창 개조"));
		case EUpgradeType::Weapon_Reload:   return FText::FromString(TEXT("재장전 개조"));
		default:                            return FText::FromString(TEXT("무기 개조"));
		}
	}
}

void UShopWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}

void UShopWidget::NativeConstruct() {
	Super::NativeConstruct();

	//����ī��
	if (CardSelectionPanel) CardSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);

	if (CardSelectionPanel)
	{
		CardSelectionPanel->OnSelectionFinishedEvent.AddDynamic(this, &UShopWidget::bRandomCardSelected);
	}

	if(UpgradeButton00)
	{
		UpgradeButton00->SetItemID(0);
		UpgradeButton00->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradePurchase);
		UpgradeButton00->SetButtonText(FText::FromString(TEXT("랜덤 카드 강화")), FText::FromString(TEXT("100")));
	}

	Char_UpgradeButtons.Empty();
	WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UShopButton* FoundButton = Cast < UShopButton>(Widget)) {
				if (FoundButton->GetName().StartsWith(TEXT("Char_UpgradeButton"))) {
					Char_UpgradeButtons.Add(FoundButton);
					int32 GeneratedID = Char_UpgradeButtons.Num() - 1;
					FoundButton->SetItemID(GeneratedID);
					FoundButton->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradCharacterState);
					FText UpgradeName;
					FText UpgradePrice = FText::FromString(TEXT("100 Gold"));
					switch (GeneratedID)
					{
					case 0:
						UpgradeName = FText::FromString(TEXT("최대 체력 증가"));
						break;
					case 1:
						UpgradeName = FText::FromString(TEXT("이동 속도 증가"));
						break;
					case 2:
						UpgradeName = FText::FromString(TEXT("체력 재생 증가"));
						break;
					default:
						UpgradeName = FText::FromString(TEXT("미정이가 간다!"));
						break;
					}
					FoundButton->SetButtonText(UpgradeName, UpgradePrice);
				}
			}
		}
	);

	Lv_Image[0] = Cast<UImage>(GetWidgetFromName(TEXT("LvHealth")));
	Lv_Image[1] = Cast<UImage>(GetWidgetFromName(TEXT("LvHealthRegen")));
	Lv_Image[2] = Cast<UImage>(GetWidgetFromName(TEXT("LvMoveSpeed")));
	Lv_Image[3] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponDamage")));
	Lv_Image[4] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponFireRate")));
	Lv_Image[5] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponRange")));
	Lv_Image[6] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponMagazine")));
	Lv_Image[7] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponReload")));
	for (int32 i = 0; i < ShopLvTotalNumber; ++i)
	{
		if (!Lv_Image[i]) continue;
		LvLinearMID[i] = Cast<UMaterialInstanceDynamic>(Lv_Image[i]->GetDynamicMaterial());
		if (LvLinearMID[i])
		{
			LvLinearMID[i]->SetScalarParameterValue(LvLinearWipeParamName, 0.f);
		}
	}

	if (Rand_UpgradeBTN00) { Rand_UpgradeBTN00->SetItemID(0); Rand_UpgradeBTN00->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleWeaponUpgradePurchase); }
	if (Rand_UpgradeBTN01) { Rand_UpgradeBTN01->SetItemID(1); Rand_UpgradeBTN01->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleWeaponUpgradePurchase); }
	if (Rand_UpgradeBTN02) { Rand_UpgradeBTN02->SetItemID(2); Rand_UpgradeBTN02->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleWeaponUpgradePurchase); }

	TryBindGameStateDelegate();
	TryBindPlayerStateDelegates();

	if (!bBoundDelegates && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			BindingTimerHandle,
			this,
			&UShopWidget::TryBindPlayerStateDelegates,
			0.1f,
			true
		);
	}
}
void UShopWidget::HandleUpgradePurchase(int32 ItemID) {


	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {
		PlayerController->Server_RequestRandomUpgradeOptions();
	}

}
//���� ĳ���� ���� ���׷��̵� ��ư ������ ����
void UShopWidget::HandleUpgradCharacterState(int32 ItemID)
{
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {

		PlayerController->Server_SelectStaticUpgradeOption(ItemID);
	}
}


void UShopWidget::Update_UpgradeSelectionWidget(const TArray<FRandomCardOption>& Options) {

	//FSlateApplication::Get().SetAllUserFocusToGameViewport();
	if (UpgradeButton00) UpgradeButton00->SetVisibility(ESlateVisibility::Collapsed);
	for (const auto& Button : Char_UpgradeButtons)
		Button->SetVisibility(ESlateVisibility::Collapsed);
	Static_Upgrade_Background->SetVisibility(ESlateVisibility::Collapsed);
	Background->SetVisibility(ESlateVisibility::Collapsed);
	BP_ShopBanner->SetVisibility(ESlateVisibility::Collapsed);
	StaticUpgradeText->SetVisibility(ESlateVisibility::Collapsed);
	if (StatLevelPanel) StatLevelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeBTN00) Rand_UpgradeBTN00->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeBTN01) Rand_UpgradeBTN01->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeBTN02) Rand_UpgradeBTN02->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeIMG00) Rand_UpgradeIMG00->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeIMG01) Rand_UpgradeIMG01->SetVisibility(ESlateVisibility::Collapsed);
	if (Rand_UpgradeIMG02) Rand_UpgradeIMG02->SetVisibility(ESlateVisibility::Collapsed);
	if (CardSelectionPanel)
	{
		CardSelectionPanel->SetCardID(Options);
		CardSelectionPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UShopWidget::bRandomCardSelected(int32 CardID)
{
	DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("bRandomCardSelected")));

	ReturnToShopButtons();
	SendToSelectionCardID(CardID);
}

void UShopWidget::ReturnToShopButtons()
{
	if (CardSelectionPanel) CardSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UpgradeButton00) UpgradeButton00->SetVisibility(ESlateVisibility::Visible);
	for (const auto& Button : Char_UpgradeButtons)
		Button->SetVisibility(ESlateVisibility::Visible);
	Static_Upgrade_Background->SetVisibility(ESlateVisibility::Visible);
	Background->SetVisibility(ESlateVisibility::Visible);
	BP_ShopBanner->SetVisibility(ESlateVisibility::Visible);
	StaticUpgradeText->SetVisibility(ESlateVisibility::Visible);
	if (StatLevelPanel) StatLevelPanel->SetVisibility(ESlateVisibility::Visible);
	UpdateWeaponUpgradeButtons();
}

void UShopWidget::SendToSelectionCardID(int32 CardID)
{
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {
		PlayerController->Server_SelectUpgradeOption(CardID);
	}
}
void UShopWidget::TryBindPlayerStateDelegates()
{
	TryBindGameStateDelegate();

	if (bBoundDelegates) return;
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC) return;
	AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
	if (PS)
	{
		PS->OnPlayerDataChangedNative.AddUObject(this, &UShopWidget::OnPlayerDataChanged);
		PS->OnGoldChnageNative.AddUObject(this, &UShopWidget::OnGoldChanged);
		PS->OnAccumulatedUpgradesChangedNative.AddUObject(this, &UShopWidget::OnAccumulatedUpgradesChanged);
		bBoundDelegates = true;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(BindingTimerHandle);
		}
		UpdateUpgradeButtons();
		UpdateStatLevelWidgets();
	}
}
void UShopWidget::UpdateUpgradeButtons()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC) return;
	AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;
	if (UpgradeButton00)
	{
		UpgradeButton00->SetButtonText(FText::FromString(TEXT("랜덤 카드 강화")), FText::FromString(TEXT("무료")));
	}
	for (UShopButton* Button : Char_UpgradeButtons)
	{
		if (!Button) continue;
		int32 ItemID = Button->GetItemID();
		EUpgradeType UpgradeType = PC->GetStaticUpgradeTypeFromIndex(ItemID);
		if (UpgradeType == EUpgradeType::None) continue;
		int32 CurrentLevel = PC->GetCurrentUpgradeLevel(PS, UpgradeType);
		int32 Cost = PC->GetStaticUpgradeCost(UpgradeType, CurrentLevel);
		FText UpgradeName;
		switch (UpgradeType)
		{
		case EUpgradeType::Player_Health:
			UpgradeName = FText::Format(FText::FromString(TEXT("최대 체력 증가 (Lv.{0})")), FText::AsNumber(CurrentLevel));
			break;
		case EUpgradeType::Player_MoveSpeed:
			UpgradeName = FText::Format(FText::FromString(TEXT("이동 속도 증가 (Lv.{0})")), FText::AsNumber(CurrentLevel));
			break;
		case EUpgradeType::Player_HealthRegeneration:
			UpgradeName = FText::Format(FText::FromString(TEXT("체력 재생 증가 (Lv.{0})")), FText::AsNumber(CurrentLevel));
			break;
		default:
			UpgradeName = FText::FromString(TEXT("스탯 강화"));
			break;
		}
		FText PriceText = FText::Format(FText::FromString(TEXT("{0} Gold")), FText::AsNumber(Cost));
		if (CurrentLevel >= 5)
		{
			Button->SetButtonText(UpgradeName, FText::FromString(TEXT("MAX")));
		}
		else
		{
			Button->SetButtonText(UpgradeName, PriceText);
		}
	}
}
void UShopWidget::OnPlayerDataChanged(const FPlayerData& NewPlayerData)
{
	UpdateUpgradeButtons();
	UpdateStatLevelWidgets();
}
void UShopWidget::OnAccumulatedUpgradesChanged(const FAccumulatedUpgrades& NewUpgrades)
{
	UpdateStatLevelWidgets();
}
void UShopWidget::UpdateStatLevelWidgets()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC) return;
	AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	const FAccumulatedUpgrades& Upgrades = PS->GetAccumulatedUpgrades();

	// 0: Health, 1: HealthRegen, 2: MoveSpeed, 3: WeaponDamage, 4: FireRate, 5: Range, 6: Magazine, 7: Reload
	if (LvLinearMID[0]) LvLinearMID[0]->SetScalarParameterValue(LvLinearWipeParamName, (PS->PlayerData.LvHealth + Upgrades.LvHealth) * 0.2f);
	if (LvLinearMID[1]) LvLinearMID[1]->SetScalarParameterValue(LvLinearWipeParamName, (PS->PlayerData.LvHealthRegeneration + Upgrades.LvHealthRegen) * 0.2f);
	if (LvLinearMID[2]) LvLinearMID[2]->SetScalarParameterValue(LvLinearWipeParamName, (PS->PlayerData.LvMovementSpeed + Upgrades.LvMoveSpeed) * 0.2f);
	if (LvLinearMID[3]) LvLinearMID[3]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponDamage * 0.2f);
	if (LvLinearMID[4]) LvLinearMID[4]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponFireRate * 0.2f);
	if (LvLinearMID[5]) LvLinearMID[5]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponRange * 0.2f);
	if (LvLinearMID[6]) LvLinearMID[6]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponMagazine * 0.2f);
	if (LvLinearMID[7]) LvLinearMID[7]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponReload * 0.2f);
}
void UShopWidget::OnGoldChanged(float NewGold)
{
	UpdateUpgradeButtons();
}
void UShopWidget::TryBindGameStateDelegate()
{
	if (bBoundGameStateDelegate) return;
	AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (!GS) return;

	GS->OnShopWeaponUpgradeOptionsChangedNative.AddUObject(this, &UShopWidget::UpdateWeaponUpgradeButtons);
	bBoundGameStateDelegate = true;
	UpdateWeaponUpgradeButtons();
}
void UShopWidget::HandleWeaponUpgradePurchase(int32 SlotIndex)
{
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController)
	{
		PlayerController->Server_PurchaseWeaponUpgrade(SlotIndex);
	}
}
void UShopWidget::UpdateWeaponUpgradeButtons()
{
	AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (!GS) return;

	UShopButton* Buttons[3] = { Rand_UpgradeBTN00, Rand_UpgradeBTN01, Rand_UpgradeBTN02 };
	UImage* Images[3] = { Rand_UpgradeIMG00, Rand_UpgradeIMG01, Rand_UpgradeIMG02 };

	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	const int32 Cost = PC ? PC->GetWeaponUpgradePurchaseCost() : 0;
	const FText PriceText = FText::Format(FText::FromString(TEXT("{0} Gold")), FText::AsNumber(Cost));

	for (int32 i = 0; i < 3; ++i)
	{
		if (!Buttons[i]) continue;

		if (!GS->ShopWeaponUpgradeOptions.IsValidIndex(i))
		{
			Buttons[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (Images[i]) Images[i]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Buttons[i]->SetVisibility(ESlateVisibility::Visible);
		if (Images[i]) Images[i]->SetVisibility(ESlateVisibility::Visible);

		const EUpgradeType Type = GS->ShopWeaponUpgradeOptions[i];
		const FText* FoundName = WeaponUpgradeNameMap.Find(Type);
		Buttons[i]->SetButtonText(FoundName ? *FoundName : GetDefaultWeaponUpgradeName(Type), PriceText);

		if (Images[i])
		{
			if (UTexture2D* const* FoundIcon = WeaponUpgradeIconMap.Find(Type))
			{
				if (*FoundIcon) Images[i]->SetBrushFromTexture(*FoundIcon);
			}
		}
	}
}