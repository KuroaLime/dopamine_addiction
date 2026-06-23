// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/GroupOwnedCard.h"
#include "Game/InGame/TPS/UI/Shop/OwnedCard.h"
#include "Game/InGame/MainPlayerState.h"


void UGroupOwnedCard::NativeConstruct()
{
    Super::NativeConstruct();

    bNeedPlayerStateBind = true;
    TryBindPlayerState();
}
void UGroupOwnedCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bNeedPlayerStateBind)
    {
        TryBindPlayerState();
    }
}
void UGroupOwnedCard::TryBindPlayerState()
{
    if (CachedPlayerState.IsValid()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    CachedPlayerState = PS;
    PS->OnOwnedCardsChangedNative.AddUObject(this, &UGroupOwnedCard::OnPlayerCardsChanged);

    if (PS->OwnedCards.Num() > 0)
    {
        OnPlayerCardsChanged(PS->OwnedCards);
    }
    bNeedPlayerStateBind = false;
}
void UGroupOwnedCard::OnPlayerCardsChanged(const TArray<FOwnedCardInfo>& NewCards)
{
    PendingCards = NewCards;
    if (CardFlipAnim)
    {
        PlayAnimation(CardFlipAnim);
    }
    else
    {
        OnCardFlipMidpoint();
    }
}

void UGroupOwnedCard::OnCardFlipMidpoint()
{
    TArray<UOwnedCard*> CardSlots = { BP_OwnedCard00, BP_OwnedCard01, BP_OwnedCard02 };
    for (int32 i = 0; i < CardSlots.Num(); ++i)
    {
        if (CardSlots[i] == nullptr) continue;
        if (i < PendingCards.Num())
        {
            UTexture2D** FoundTexture = CardTextureMap.Find(PendingCards[i].CardID);
            if (FoundTexture && *FoundTexture)
            {
                CardSlots[i]->SetCardData(PendingCards[i], *FoundTexture);
                CardSlots[i]->SetVisibility(ESlateVisibility::Visible);
            }
        }
        else
        {
            CardSlots[i]->ClearCard();
            CardSlots[i]->SetVisibility(ESlateVisibility::Visible);
        }
    }
}