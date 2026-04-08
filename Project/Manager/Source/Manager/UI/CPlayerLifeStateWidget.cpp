// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CPlayerLifeStateWidget.h"
#include "Data/CharacterStateComponent.h"
#include "Components/Image.h"

void UCPlayerLifeStateWidget::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	CurrentCharacterState = NewCharacterState;
	//NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);
	//NewCharacterState->OnLEVELChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevelWidget);

	//UpdatePSU_BackgroundImage();
	//UpdatePlayerImage();
}

void UCPlayerLifeStateWidget::NativeConstruct() {
	Super::NativeConstruct();

	PSU_Background[0] = Cast<UImage>(GetWidgetFromName(TEXT("BPlayer_StateUI00")));
	PSU_Background[1] = Cast<UImage>(GetWidgetFromName(TEXT("BPlayer_StateUI01")));

	Player[0] = Cast<UImage>(GetWidgetFromName(TEXT("Player00")));
	Player[1] = Cast<UImage>(GetWidgetFromName(TEXT("Player01")));
	Player[2] = Cast<UImage>(GetWidgetFromName(TEXT("Player02")));
	Player[3] = Cast<UImage>(GetWidgetFromName(TEXT("Player03")));

	UpdatePSU_BackgroundImage();
	UpdatePlayerImage();
}

void UCPlayerLifeStateWidget::UpdatePSU_BackgroundImage() {
	if (CurrentCharacterState.IsValid()) {
		//if (nullptr != GoldBackgroundImage) GoldBackgroundImage->SetBrushFromTexture("ddf");
	}
}

void UCPlayerLifeStateWidget::UpdatePlayerImage() {
	if (CurrentCharacterState.IsValid()) {
		//if (nullptr != GoldBackgroundImage) GoldBackgroundImage->SetBrushFromTexture("ddf");
	}
}