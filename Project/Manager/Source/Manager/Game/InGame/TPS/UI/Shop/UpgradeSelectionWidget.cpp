// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/UpgradeSelectionWidget.h"
#include "Game/InGame/TPS/UI/Shop/CardWidget.h"
void UUpgradeSelectionWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}

void UUpgradeSelectionWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (SelectionCard00) SelectionCard00->OnCardSelectionEvent.AddDynamic(this, &UUpgradeSelectionWidget::HandleCardSelected);
	if (SelectionCard01) SelectionCard01->OnCardSelectionEvent.AddDynamic(this, &UUpgradeSelectionWidget::HandleCardSelected);
	if (SelectionCard02) SelectionCard02->OnCardSelectionEvent.AddDynamic(this, &UUpgradeSelectionWidget::HandleCardSelected);


}

void UUpgradeSelectionWidget::HandleCardSelected(int32 CardID) {

	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	if (OnSelectionFinishedEvent.IsBound())
	{
		OnSelectionFinishedEvent.Broadcast();
	}
}


void UUpgradeSelectionWidget::SetCardID(int32 NewID) {
	if (SelectionCard00) SelectionCard00->SetCardID(101);
	if (SelectionCard01) SelectionCard01->SetCardID(102);
	if (SelectionCard02) SelectionCard02->SetCardID(103);
}
void UUpgradeSelectionWidget::UpdateWidget() {

}