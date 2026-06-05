// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"
#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/InGame/TPS/UI/Shop/UpgradeSelectionWidget.h"

void UShopWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}

void UShopWidget::NativeConstruct() {
	Super::NativeConstruct();


	if (CardSelectionPanel) CardSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);

	if (CardSelectionPanel)
	{
		CardSelectionPanel->OnSelectionFinishedEvent.AddDynamic(this, &UShopWidget::ReturnToShopButtons);
	}

	if(UpgradeButton00)
	{
		UpgradeButton00->SetItemID(0);
		UpgradeButton00->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradePurchase);
	}
	if (UpgradeButton01)
	{
		UpgradeButton01->SetItemID(1);
		UpgradeButton01->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradePurchase);
	}
	if (UpgradeButton02)
	{
		UpgradeButton02->SetItemID(2);
		UpgradeButton02->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradePurchase);
	}


}
void UShopWidget::HandleUpgradePurchase(int32 ItemID) {
	switch (ItemID) {
	case 0:
		UE_LOG(LogTemp, Warning, TEXT("0번 아이템(UpgradButton00) 구매 시도!"));
		break;
	case 1:
		UE_LOG(LogTemp, Warning, TEXT("1번 아이템(UpgradButton00) 구매 시도!"));
		break;
	case 2:
		UE_LOG(LogTemp, Warning, TEXT("2번 아이템(UpgradButton00) 구매 시도!"));
		break;

	default:
		break;
	}

	if (UpgradeButton00) UpgradeButton00->SetVisibility(ESlateVisibility::Collapsed);
	if (UpgradeButton01) UpgradeButton01->SetVisibility(ESlateVisibility::Collapsed);
	if (UpgradeButton02) UpgradeButton02->SetVisibility(ESlateVisibility::Collapsed);

	if (CardSelectionPanel)
	{
		CardSelectionPanel->SetCardID(ItemID);
		CardSelectionPanel->SetVisibility(ESlateVisibility::Visible);
	}

	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void UShopWidget::UpdateWidget() {

}
void UShopWidget::ReturnToShopButtons()
{
	if (CardSelectionPanel) CardSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UpgradeButton00) UpgradeButton00->SetVisibility(ESlateVisibility::Visible);
	if (UpgradeButton01) UpgradeButton01->SetVisibility(ESlateVisibility::Visible);
	if (UpgradeButton02) UpgradeButton02->SetVisibility(ESlateVisibility::Visible);
}