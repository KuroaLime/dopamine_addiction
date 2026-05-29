// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/UI/CardPlayerMainHUD.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Game/InGame/TPS/UI/CRoundandTimerWidget.h"

void UCardPlayerMainHUD::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	CurrentCharacterState = NewCharacterState;
	//NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);
	//NewCharacterState->OnLEVELChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevelWidget);

	if (CRoundandTimer_UI)
	{
		CRoundandTimer_UI->BindCharacterState(NewCharacterState);
	}
	StaticUI();
	UpdateHoldingGoldText();
	UpdateCPlayers_State_UI();
	UpdateCRoundandTimer_UI();
}

void UCardPlayerMainHUD::NativeConstruct() {
	Super::NativeConstruct();

	GoldBackgroundImage = Cast<UImage>(GetWidgetFromName(TEXT("HoldingGold_Image")));
	HoldingGoldText = Cast<UTextBlock>(GetWidgetFromName(TEXT("HoldingGold_Text")));
	CPlayersStateUI = Cast<UUserWidget>(GetWidgetFromName(TEXT("CPlayers_State_UI")));
	CRoundandTimerUI = Cast<UUserWidget>(GetWidgetFromName(TEXT("CRoundandTimer_UI")));


}
void UCardPlayerMainHUD::StaticUI() {
	UpdateGoldBackgroundImage();
	UpdateGoldBackgroundImage();
	UpdateCPlayers_State_UI();
}

void UCardPlayerMainHUD::UpdateGoldBackgroundImage() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != GoldBackgroundImage) GoldBackgroundImage->SetBrushFromTexture(GoldBackground_Image);
	}
}
void UCardPlayerMainHUD::UpdateHoldingGoldText() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != HoldingGoldText) {
			HoldingGoldText->SetText(FText::AsNumber(CurrentCharacterState->GetGold()));
		}
	}
}
void UCardPlayerMainHUD::UpdateCPlayers_State_UI() {
	if (CurrentCharacterState.IsValid()) {
		UImage* PSU_LEFT = Cast<UImage>(CPlayersStateUI->GetWidgetFromName(TEXT("BPlayer_StateUI00")));
		if (nullptr != PSU_LEFT) PSU_LEFT->SetBrushFromTexture(PLS_Images[0]);
		
		UImage* PSU_RIGHT = Cast<UImage>(CPlayersStateUI->GetWidgetFromName(TEXT("BPlayer_StateUI01")));
		if (nullptr != PSU_RIGHT) PSU_RIGHT->SetBrushFromTexture(PLS_Images[1]);

		UImage* PSU_MIDDLE = Cast<UImage>(CPlayersStateUI->GetWidgetFromName(TEXT("PSU_RoundAndTimer")));
		if (nullptr != PSU_MIDDLE) PSU_MIDDLE->SetBrushFromTexture(RoundAndTimer_Image);
	}

}
void UCardPlayerMainHUD::UpdateCRoundandTimer_UI() {
	//CRoundandTimer_UI->UpdateTimer_TextImage(1);
}


//
///////////////////
