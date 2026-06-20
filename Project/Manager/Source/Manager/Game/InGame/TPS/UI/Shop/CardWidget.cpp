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
	if (OnCardSelectionEvent.IsBound())
		OnCardSelectionEvent.Broadcast(SelectionIndex);

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
