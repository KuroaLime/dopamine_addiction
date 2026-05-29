// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/TpsCharacterWidget.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"

void UTpsCharacterWidget::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	CurrentCharacterState = NewCharacterState;
	NewCharacterState->OnHPChanged.AddUObject(this, &UTpsCharacterWidget::UpdateHPWidget);
	UpdateHPWidget();
}

void UTpsCharacterWidget::NativeConstruct() {
	Super::NativeConstruct();

	HPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_HPBar")));
	
	//UpdateHPWidget();
}

void UTpsCharacterWidget::UpdateHPWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != HPProgressBar) HPProgressBar->SetPercent(CurrentCharacterState->GetHPRatio());
	}
}
