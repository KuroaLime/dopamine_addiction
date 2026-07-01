// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/Shop/OwnedCard.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Game/InGame/MainGameState.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Game/InGame/MainPlayerController.h"

void UOwnedCard::NativeConstruct() {
	Super::NativeConstruct();

	/*if (Selection_Button)
		Selection_Button->OnClicked.AddDynamic(this, &UCardWidget::OnSelectCardClicked);*/

	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}
	if (Selection_Button)
	{
		Selection_Button->OnClicked.AddDynamic(this, &UOwnedCard::OnCardButtonClicked);
	}
}





void UOwnedCard::SetUpgradeType(int32 CardValue, int32 Index) {
	//SelectionIndex = Index;
	//CurCardValue = CardValue;
	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}

}

void UOwnedCard::SetCardData(const FOwnedCardInfo& CardInfo, class UTexture2D* CardTexture)
{
	CachedCardInfo.CardInstanceId = CardInfo.CardInstanceId;

	CachedCardInfo.CardID = CardInfo.CardID;
	CardFrontTexture = CardTexture;
	bIsFaceUp = true;
	if (Selection_Backgorund && CardFrontTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardFrontTexture);
	}
	SetRenderScale(FVector2D(1.f, 1.f));
}
void UOwnedCard::ClearCard()
{
	CachedCardInfo.CardInstanceId = 0;
	CachedCardInfo.CardID = ECardID::None;
	bIsFaceUp = false;

	if (Selection_Backgorund && CardBackTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(CardBackTexture);
	}
	SetRenderScale(FVector2D(1.f, 1.f));
}
void UOwnedCard::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	bool bIsFlipping = GetWorld()->GetTimerManager().IsTimerActive(ClickSequenceTimerHandle) ||
		(CardFlipAnim && IsAnimationPlaying(CardFlipAnim));
	// 뒤집히는 중이 아닐 때만 호버 확대 애니메이션 재생
	if (bIsFaceUp && HoverAnim && !bIsFlipping) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Forward);
	}
}
void UOwnedCard::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	bool bIsFlipping = GetWorld()->GetTimerManager().IsTimerActive(ClickSequenceTimerHandle) ||
		(CardFlipAnim && IsAnimationPlaying(CardFlipAnim));
	// 뒤집히는 중이 아닐 때만 호버 축소 애니메이션 재생
	if (bIsFaceUp && HoverAnim && !bIsFlipping) {
		PlayAnimation(HoverAnim, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}

void UOwnedCard::OnCardButtonClicked()
{
	if (CachedCardInfo.CardID == ECardID::None) return;

	if (ClickAnim && !IsAnimationPlaying(ClickAnim) && (!CardFlipAnim || !IsAnimationPlaying(CardFlipAnim)))
	{
		if (bIsFaceUp)
		{
			PlayAnimation(ClickAnim);
			float ClickAnimDuration = ClickAnim->GetEndTime();
			GetWorld()->GetTimerManager().SetTimer(
				ClickSequenceTimerHandle,
				this,
				&UOwnedCard::PlayFlipAnimation,
				ClickAnimDuration,
				false
			);

		}
		else
		{
			PlayFlipAnimation();
		}
	}
}


void UOwnedCard::PlayFlipAnimation()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("PlayFlipAnimation Called!"));
	}
	if (ClickAnim)
	{
		StopAnimation(ClickAnim);
	}
	if (CardFlipAnim)
	{
		EUMGSequencePlayMode::Type PlayMode = bIsFaceUp ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse;
		PlayAnimation(CardFlipAnim, 0.f, 1, PlayMode);
	}
}
void UOwnedCard::OnCardFlipMidpoint()
{
	bIsFaceUp = !bIsFaceUp;
	UTexture2D* TargetTexture = bIsFaceUp ? CardFrontTexture : CardBackTexture;
	if (Selection_Backgorund && TargetTexture)
	{
		Selection_Backgorund->SetBrushFromTexture(TargetTexture);
	}
	if (!bIsFaceUp && CachedCardInfo.CardInstanceId > 0)
	{
		AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
		if (PC)
		{
			PC->Server_RequestDiscardCard(CachedCardInfo.CardInstanceId);
		}
	}
}