// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/GroupOwnedCard.h"
#include "Game/InGame/TPS/UI/Shop/OwnedCard.h"
#include "Game/InGame/Card/Data/CardTextureSet.h"
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
            // None(빈 카드)은 앞면 표시 대상 아님 — 기존 맵 미등록 시 스킵과 동일 동작
            UTexture2D* FrontTexture =
                (CardTextures && PendingCards[i].CardID != ECardID::None)
                    ? CardTextures->GetFront(PendingCards[i].CardID) : nullptr;
            if (FrontTexture)
            {
                CardSlots[i]->SetCardData(PendingCards[i], FrontTexture);
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