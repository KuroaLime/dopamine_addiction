// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/CardWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
void UCardWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}
void UCardWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Selection_Button)
		Selection_Button->OnClicked.AddDynamic(this, &UCardWidget::OnSelectCardClicked);
}



void UCardWidget::OnSelectCardClicked() {
	if (OnCardSelectionEvent.IsBound())
		OnCardSelectionEvent.Broadcast(SelectionIndex);

}
void UCardWidget::OnSelectCardHover() {
	//카드가 커졌다작아졌다~
}
void UCardWidget::UpdateWidget() {
	//물결이 차오르는 듯한 표현 추가 필요
}
void UCardWidget::SetUpgradeType(EUpgradeType NewType, int32 Index) {
	CurrentType = NewType;
	SelectionIndex = Index;

	//여기서 데이터테이블 연계 확인
}
