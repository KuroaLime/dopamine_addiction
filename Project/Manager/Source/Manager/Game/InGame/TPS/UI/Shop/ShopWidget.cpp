// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"
#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/InGame/TPS/UI/Shop/UpgradeSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/TPS/UI/Shop/ShopBanner.h"


void UShopWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}

void UShopWidget::NativeConstruct() {
	Super::NativeConstruct();

	//랜덤카드
	if (CardSelectionPanel) CardSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);

	if (CardSelectionPanel)
	{
		CardSelectionPanel->OnSelectionFinishedEvent.AddDynamic(this, &UShopWidget::bRandomCardSelected);
	}

	if(UpgradeButton00)
	{
		UpgradeButton00->SetItemID(0);
		UpgradeButton00->OnPurchaseEvent.AddDynamic(this, &UShopWidget::HandleUpgradePurchase);
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

				}
			}
		}
	);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Total %d upgrade buttons registered."), Char_UpgradeButtons.Num()));
	


}
void UShopWidget::HandleUpgradePurchase(int32 ItemID) {


	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {
		PlayerController->Server_RequestRandomUpgradeOptions();
	}

}
//고정 캐릭터 스탯 업그레이드 버튼 누를시 수행
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

	if (CardSelectionPanel)
	{
		CardSelectionPanel->SetCardID(Options); //고르는 카드 ID 설정(오해 ㄴㄴ염)
		CardSelectionPanel->SetVisibility(ESlateVisibility::Visible);//보이게
	}
}

void UShopWidget::bRandomCardSelected(int32 CardID)
{
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("bRandomCardSelected")));

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
}

void UShopWidget::SendToSelectionCardID(int32 CardID)
{
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {
		PlayerController->Server_SelectUpgradeOption(CardID);
	}
}
