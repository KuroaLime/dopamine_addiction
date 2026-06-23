// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/CardWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Game/InGame/MainGameState.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"


void UCardWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}
void UCardWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Selection_Button)
		Selection_Button->OnClicked.AddDynamic(this, &UCardWidget::OnSelectCardClicked);

	bIsFaceUp = true;
	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}
	if (Selection_Icon) Selection_Icon->SetVisibility(ESlateVisibility::Collapsed);
	if (Card_Name) Card_Name->SetVisibility(ESlateVisibility::Collapsed);
	if (Card_Descriptor) Card_Descriptor->SetVisibility(ESlateVisibility::Collapsed);
}



void UCardWidget::OnSelectCardClicked() {
	if (bIsFaceUp) {
		if (FlipAnim && !IsAnimationPlaying(FlipAnim)) {
			PlayAnimation(FlipAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
			bIsFaceUp = false;
		}
	}
	else {
		if (SelectAnim && !IsAnimationPlaying(SelectAnim)) {
			PlayAnimationTimeRange(SelectAnim, 0.f, SelectAnim->GetEndTime(), 1, EUMGSequencePlayMode::Forward, 1.f, true);

			float Delay = SelectAnim->GetEndTime();
			GetWorld()->GetTimerManager().SetTimer(SelectTimerHandle, this, &UCardWidget::BroadcastSelectionEvent, Delay, false);
		}
		else {
			BroadcastSelectionEvent();
		}
	}
}

void UCardWidget::BroadcastSelectionEvent()
{
	bIsFaceUp = true;
	if (OnCardSelectionEvent.IsBound()) {
		OnCardSelectionEvent.Broadcast(SelectionIndex);
	}
}
void UCardWidget::OnSelectCardHover() {
	//카드가 커졌다작아졌다~
}
void UCardWidget::UpdateWidget() {
	//물결이 차오르는 듯한 표현 추가 필요
}
void UCardWidget::SetUpgradeType(const FRandomCardOption& NewOption, int32 Index) {
	CurrentOption = NewOption;
	SelectionIndex = Index;
	bIsFaceUp = true;
	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}
	DefaultStartState();
	if (Selection_Icon) Selection_Icon->SetVisibility(ESlateVisibility::Collapsed);
	if (Card_Name) Card_Name->SetVisibility(ESlateVisibility::Collapsed);
	if (Card_Descriptor) Card_Descriptor->SetVisibility(ESlateVisibility::Collapsed);

    AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    if (!GS) return;
    UDataTable* ShopTable = GS->GetShopRandomCardDataTable();
    if (!ShopTable) return;
    FRandomUpgradeCardDataTable* CardData = ShopTable->FindRow<FRandomUpgradeCardDataTable>(CurrentOption.CardRowName, TEXT("Context_CardUI"));
    if (!CardData) return;
    if (Card_Name) Card_Name->SetText(CardData->CardTitle);
    if (Selection_Icon) Selection_Icon->SetBrushFromTexture(CardData->CardTexture);
    if (Card_Descriptor)
    {
        FString FullDesc = CardData->CardDescription.ToString() + TEXT("\n\n");
        for (const auto& Pair : CurrentOption.RolledStats)
        {
            FString StatName = UEnum::GetValueAsString(Pair.Key);
            StatName.Split(TEXT("::"), nullptr, &StatName);
            FullDesc += FString::Printf(TEXT("%s: +%.1f\n"), *StatName, Pair.Value);
        }
        Card_Descriptor->SetText(FText::FromString(FullDesc));
    }
}
void UCardWidget::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	if (bIsFaceUp && HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
	}
}
void UCardWidget::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	if (bIsFaceUp && HoverAnim) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}
FReply UCardWidget::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Mouse Entered Card Widget!"));
	}
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		
		if (FlipAnim && !IsAnimationPlaying(FlipAnim)) {
			if (bIsFaceUp) {
				PlayAnimation(FlipAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
			}
			else {
				PlayAnimation(FlipAnim, 0.f, 1, EUMGSequencePlayMode::Reverse);
			}
			bIsFaceUp = !bIsFaceUp;
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(MyGeometry, MouseEvent);
}

void UCardWidget::OnFlipAnimMidpoint()
{
	if (Selection_Icon) {
		if (bIsFaceUp) {
			if (Selection_Backgorund && CardFrontTexture) {
				Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
			}
			if (Selection_Icon) Selection_Icon->SetVisibility(ESlateVisibility::Collapsed);
			if (Card_Name) Card_Name->SetVisibility(ESlateVisibility::Collapsed);
			if (Card_Descriptor) Card_Descriptor->SetVisibility(ESlateVisibility::Collapsed);
		}
		else {
			

			if (Selection_Backgorund && CardBackTexture) {
				Selection_Backgorund->SetBrushFromTexture(CardBackTexture);
			}
			if (Selection_Icon) Selection_Icon->SetVisibility(ESlateVisibility::Visible);
			if (Card_Name) Card_Name->SetVisibility(ESlateVisibility::Visible);
			if (Card_Descriptor) Card_Descriptor->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void UCardWidget::DefaultStartState()
{
	if (SelectAnim)
	{
		StopAnimation(SelectAnim);
	}
	if (FlipAnim)
	{
		StopAnimation(FlipAnim);
	}
	
	SetRenderScale(FVector2D(1.f, 1.f));
	SetRenderOpacity(1.f);
	SetRenderTranslation(FVector2D(0.f, 0.f));
}
