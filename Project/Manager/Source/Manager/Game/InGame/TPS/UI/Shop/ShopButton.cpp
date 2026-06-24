// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/ShopButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
void UShopButton::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}
void UShopButton::NativeConstruct() {
	Super::NativeConstruct();

	if (Purchase_Button)
		Purchase_Button->OnClicked.AddDynamic(this, &UShopButton::OnPurchaseButtonClicked);
}



void UShopButton::OnPurchaseButtonClicked() {
	if (ClickAnim)
	{
		PlayAnimation(ClickAnim);
		float AnimDuration = ClickAnim->GetEndTime();
		FTimerHandle PurchaseTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			PurchaseTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (OnPurchaseEvent.IsBound())
					{
						OnPurchaseEvent.Broadcast(ButtonItemID);
					}
				}),
			AnimDuration,
			false
		);
	}
	else
	{
		OnPurchaseEvent.Broadcast(ButtonItemID);
	}

}
void UShopButton::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Mouse Entered Card Widget!"));
	}
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	if (HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
	}
}
void UShopButton::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	if (HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}
//FReply UShopButton::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
//{
//	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
//		if (ClickAnim && !IsAnimationPlaying(ClickAnim)) {
//			PlayAnimation(ClickAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
//			return FReply::Handled();
//		}
//	}
//	return Super::NativeOnMouseButtonDown(MyGeometry, MouseEvent);
//}
void UShopButton::UpdateWidget() {
	//물결이 차오르는 듯한 표현 추가 필요
}
void UShopButton::SetItemID(int32 NewID) {
	ButtonItemID = NewID;
}