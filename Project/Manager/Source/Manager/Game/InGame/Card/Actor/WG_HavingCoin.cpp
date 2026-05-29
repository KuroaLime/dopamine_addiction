// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/Actor/WG_HavingCoin.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/TextBlock.h"

void UWG_HavingCoin::BindCharacterState(UCharacterStateComponent* NewCharacterState) {
	
	CurrentCharacterState = NewCharacterState;
	NewCharacterState->OnGoldChanged.AddUObject(this, &UWG_HavingCoin::UpdateHavingCoinWidget);
	UpdateHavingCoinWidget();
}

void UWG_HavingCoin::NativeConstruct() {
	Super::NativeConstruct();

	HavingCoinText = Cast<UTextBlock>(GetWidgetFromName(TEXT("CoinFront")));
	UpdateHavingCoinWidget();
}
void UWG_HavingCoin::NativeDestruct()
{
	Super::NativeDestruct();
	if (CurrentCharacterState.IsValid())
	{
		CurrentCharacterState->OnGoldChanged.RemoveAll(this);
	}
}

void UWG_HavingCoin::UpdateHavingCoinWidget() {
	
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != HavingCoinText) HavingCoinText->SetText(FText::AsNumber(CurrentCharacterState->GetGold()));
	}
}
