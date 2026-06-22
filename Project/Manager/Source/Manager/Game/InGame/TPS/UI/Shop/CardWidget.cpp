// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/CardWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Game/InGame/MainGameState.h"


void UCardWidget::BindCharacterState(class UCharacterStateComponent* NewCharacterState) {

}
void UCardWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Selection_Button)
		Selection_Button->OnClicked.AddDynamic(this, &UCardWidget::OnSelectCardClicked);
}



void UCardWidget::OnSelectCardClicked() {
	if (bIsFaceUp) {
		if (FlipAnim && !IsAnimationPlaying(FlipAnim)) {
			PlayAnimation(FlipAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
			bIsFaceUp = false;
		}
	}
	else {
		if (OnCardSelectionEvent.IsBound()) {
			bIsFaceUp = true;
			OnCardSelectionEvent.Broadcast(SelectionIndex);
		}
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