// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"
#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/InGame/TPS/UI/Shop/UpgradeSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/TPS/UI/Shop/ShopBanner.h"


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
					FText UpgradePrice = FText::FromString(TEXT("50 Gold"));
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
	if (CardSelectionPanel)
	{
		CardSelectionPanel->SetCardID(Options);
		CardSelectionPanel->SetVisibility(ESlateVisibility::Visible);
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
	StaticUpgradeText->SetVisibility(ESlateVisibility::Visible);
}

void UShopWidget::SendToSelectionCardID(int32 CardID)
{
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetOwningPlayer());
	if (PlayerController) {
		PlayerController->Server_SelectUpgradeOption(CardID);
	}
}
