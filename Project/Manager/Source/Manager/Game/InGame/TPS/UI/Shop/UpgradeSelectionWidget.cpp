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


		OnSelectionFinishedEvent.Broadcast(CardID);
	}
}

//여기 생성한 카드 번호를 받아와서 set 해주기
void UUpgradeSelectionWidget::SetCardID(const TArray<EUpgradeType>& Options) {
	if (SelectionCard00) SelectionCard00->SetUpgradeType(Options[0],0);
	if (SelectionCard01) SelectionCard01->SetUpgradeType(Options[1], 1);
	if (SelectionCard02) SelectionCard02->SetUpgradeType(Options[2], 2);
	FString EnumString = UEnum::GetValueAsString(Options[0]);
	FString EnumString1 = UEnum::GetValueAsString(Options[1]);
	FString EnumString2 = UEnum::GetValueAsString(Options[2]);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, EnumString);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, EnumString1);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, EnumString2);


}
void UUpgradeSelectionWidget::UpdateWidget() {

}