// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
void UShopButton::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}
void UShopButton::NativeConstruct() {
	Super::NativeConstruct();

	if (Purchase_Button)
		Purchase_Button->OnClicked.AddDynamic(this, &UShopButton::OnPurchaseButtonClicked);
}



void UShopButton::OnPurchaseButtonClicked() {
	if (OnPurchaseEvent.IsBound())
		OnPurchaseEvent.Broadcast(ButtonItemID);

}
void UShopButton::UpdateWidget() {
	//물결이 차오르는 듯한 표현 추가 필요
}
void UShopButton::SetItemID(int32 NewID) {
	ButtonItemID = NewID;
}