// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/OwnedCard.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Game/InGame/MainGameState.h"


void UOwnedCard::NativeConstruct() {
	Super::NativeConstruct();

	/*if (Selection_Button)
		Selection_Button->OnClicked.AddDynamic(this, &UCardWidget::OnSelectCardClicked);*/

	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}
}





void UOwnedCard::SetUpgradeType(int32 CardValue, int32 Index) {
	SelectionIndex = Index;
	CurCardValue = CardValue;
	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}

}
void UOwnedCard::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	if (HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
	}
}
void UOwnedCard::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	if (HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}
FReply UOwnedCard::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {

		if (ClickAnim && !IsAnimationPlaying(ClickAnim)) {
			PlayAnimation(ClickAnim, 0.f, 1, EUMGSequencePlayMode::Forward);

			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(MyGeometry, MouseEvent);
}



