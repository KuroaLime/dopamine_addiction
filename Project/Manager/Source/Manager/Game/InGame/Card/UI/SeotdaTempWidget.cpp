#include "Game/InGame/Card/UI/SeotdaTempWidget.h"
#include "Game/InGame/Card/Data/CardTextureSet.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/SlateTypes.h"

namespace
{
	constexpr int32 MaxDisplayedPlayerNameLength = 10;

	FString TruncateDisplayName(const FString& PlayerName)
	{
		return PlayerName.Left(MaxDisplayedPlayerNameLength);
	}

	UTexture2D* GetRoundIconTexture(int32 Round)
	{
		static TSoftObjectPtr<UTexture2D> RoundIcons[] = {
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_01.Round_01"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_02.Round_02"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_03.Round_03"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_04.Round_04")))
		};

		return Round >= 1 && Round <= UE_ARRAY_COUNT(RoundIcons)
			? RoundIcons[Round - 1].LoadSynchronous()
			: nullptr;
	}

	// "8월", "1월 광"처럼 짧게 — Gwang(광)은 이 20장짜리 섯다패에서 1/3/8월 셋뿐이다.
	FString FormatCardShortLabel(ECardID CardID)
	{
		if (CardID == ECardID::None)
		{
			return FString();
		}

		const uint8 Raw = static_cast<uint8>(CardID);
		const int32 Month = (Raw + 1) / 2;
		const bool bGwang = (CardID == ECardID::Jan_Gwang || CardID == ECardID::Mar_Gwang || CardID == ECardID::Aug_Gwang);

		return bGwang
			? FString::Printf(TEXT("%d월 광"), Month)
			: FString::Printf(TEXT("%d월"), Month);
	}

	void EnsureWidgetHierarchyVisible(UWidget* Widget, int32 ParentDepth)
	{
		if (!Widget)
		{
			return;
		}

		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		Widget->SetRenderOpacity(1.0f);

		UWidget* Parent = Widget->GetParent();
		for (int32 Depth = 0; Parent && Depth < ParentDepth; ++Depth)
		{
			Parent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Parent->SetRenderOpacity(1.0f);
			Parent = Parent->GetParent();
		}
	}
}

void USeotdaTempWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RoundTxt)
	{
		RoundTxt->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!CardTextures)
	{
		CardTextures = UCardTextureSet::LoadDefault();
	}

	// BindWidget으로 자동 해석된 좌석 포인터를, 인덱스 순회가 필요한 로직을 위해 편의 배열에 채워 넣는다.
	SeatBackgrounds[0] = Seat0Bg;
	SeatBackgrounds[1] = Seat1Bg;
	SeatBackgrounds[2] = Seat2Bg;
	SeatBackgrounds[3] = Seat3Bg;

	SeatNameTexts[0] = Seat0Name;
	SeatNameTexts[1] = Seat1Name;
	SeatNameTexts[2] = Seat2Name;
	SeatNameTexts[3] = Seat3Name;

	SeatChipTexts[0] = Seat0Chip;
	SeatChipTexts[1] = Seat1Chip;
	SeatChipTexts[2] = Seat2Chip;
	SeatChipTexts[3] = Seat3Chip;

	SeatTurnHighlights[0] = Seat0Turn;
	SeatTurnHighlights[1] = Seat1Turn;
	SeatTurnHighlights[2] = Seat2Turn;
	SeatTurnHighlights[3] = Seat3Turn;

	SeatCardImages[0] = Seat0Card;
	SeatCardImages[1] = Seat1Card;
	SeatCardImages[2] = Seat2Card;
	SeatCardImages[3] = Seat3Card;

	BindButtonEvents();
	ApplyButtonSkin();
	if (LobbyBtn)
	{
		LobbyBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnLobbyClicked);
	}
	if (const AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		LastHandledRevealResultSerial = PC->SeotdaUiRevealResultSerial;
		LastHandledSelectionResultSerial = PC->SeotdaUiSelectionResultSerial;
	}
	RefreshFromPlayerState();
}

void USeotdaTempWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshFromPlayerState();
}

void USeotdaTempWidget::BindButtonEvents()
{
	// Card Selection Events
	if (Card0Btn)
	{
		Card0Btn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
		Card0Btn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
		Card0Btn->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Hovered);
		Card0Btn->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard0Hovered);
		Card0Btn->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Unhovered);
		Card0Btn->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard0Unhovered);
	}
	if (Card1Btn)
	{
		Card1Btn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
		Card1Btn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
		Card1Btn->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Hovered);
		Card1Btn->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard1Hovered);
		Card1Btn->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Unhovered);
		Card1Btn->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard1Unhovered);
	}
	if (Card2Btn)
	{
		Card2Btn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
		Card2Btn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
		Card2Btn->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Hovered);
		Card2Btn->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard2Hovered);
		Card2Btn->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Unhovered);
		Card2Btn->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard2Unhovered);
	}

	// Submit Button
	if (SubmitBtn)
	{
		SubmitBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
		SubmitBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
	}

	// Betting Button Events
	if (CheckBtn)
	{
		CheckBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCheckClicked);
		CheckBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCheckClicked);
	}
	if (CallBtn)
	{
		CallBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCallClicked);
		CallBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCallClicked);
	}
	if (QuarterBtn)
	{
		QuarterBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnQuarterClicked);
		QuarterBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnQuarterClicked);
	}
	if (HalfBtn)
	{
		HalfBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnHalfClicked);
		HalfBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnHalfClicked);
	}
	if (DdadangBtn)
	{
		DdadangBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnDdadangClicked);
		DdadangBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnDdadangClicked);
	}
	if (PpingBtn)
	{
		PpingBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnPpingClicked);
		PpingBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnPpingClicked);
	}
	if (AllInBtn)
	{
		AllInBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnAllInClicked);
		AllInBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnAllInClicked);
	}
	if (DieBtn)
	{
		DieBtn->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnDieClicked);
		DieBtn->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnDieClicked);
	}
}

void USeotdaTempWidget::ApplyButtonSkin()
{
	if (!ButtonNormalTexture && !ButtonHoveredTexture && !ButtonPressedTexture)
	{
		return;
	}

	FButtonStyle Style;
	if (ButtonNormalTexture)
	{
		Style.Normal.SetResourceObject(ButtonNormalTexture);
		Style.Disabled.SetResourceObject(ButtonNormalTexture);
	}
	if (ButtonHoveredTexture)
	{
		Style.Hovered.SetResourceObject(ButtonHoveredTexture);
	}
	if (ButtonPressedTexture)
	{
		Style.Pressed.SetResourceObject(ButtonPressedTexture);
	}

	UButton* AllButtons[] = {
		Card0Btn, Card1Btn, Card2Btn, SubmitBtn,
		CheckBtn, CallBtn, QuarterBtn, HalfBtn, DdadangBtn, PpingBtn, AllInBtn, DieBtn,
		LobbyBtn
	};
	for (UButton* Btn : AllButtons)
	{
		if (Btn)
		{
			Btn->SetStyle(Style);
		}
	}
}

void USeotdaTempWidget::ResetLocalRoundUiState(const TArray<FOwnedCardInfo>& Cards)
{
	TArray<int32> CurrentIds;
	CurrentIds.Reserve(Cards.Num());

	for (const FOwnedCardInfo& CardInfo : Cards)
	{
		CurrentIds.Add(CardInfo.CardInstanceId);
	}

	if (CurrentIds == LastSeenCardInstanceIds)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CL] SeotdaTempWidget LocalStateReset Old=[%s] New=[%s]"),
		*BuildCardIdListString(LastSeenCardInstanceIds),
		*BuildCardIdListString(CurrentIds));

	LastSeenCardInstanceIds = CurrentIds;
	ClearLocalCardSelection();
	bLocalRevealPending = false;
	bLocalSelectionPending = false;
	PendingLocalRevealCardID = ECardID::None;
	ConfirmedLocalRevealCardID = ECardID::None;
	bLastKnownRevealConfirmed = false;
	LastBetActionTimeSeconds = -1000.0;

	SetCardSelectionButtonsEnabled(Cards.Num() >= 3);
	SetBetButtonsEnabled(false);

	if (SubmitBtn)
	{
		SubmitBtn->SetIsEnabled(false);
	}
}

void USeotdaTempWidget::SetCardSelectionButtonsEnabled(bool bEnabled)
{
	if (Card0Btn) Card0Btn->SetIsEnabled(bEnabled);
	if (Card1Btn) Card1Btn->SetIsEnabled(bEnabled);
	if (Card2Btn) Card2Btn->SetIsEnabled(bEnabled);
}

void USeotdaTempWidget::SetBetButtonsEnabled(bool bEnabled)
{
	if (CheckBtn) CheckBtn->SetIsEnabled(bEnabled);
	if (CallBtn) CallBtn->SetIsEnabled(bEnabled);
	if (QuarterBtn) QuarterBtn->SetIsEnabled(bEnabled);
	if (HalfBtn) HalfBtn->SetIsEnabled(bEnabled);
	if (DdadangBtn) DdadangBtn->SetIsEnabled(bEnabled);
	if (PpingBtn) PpingBtn->SetIsEnabled(bEnabled);
	if (AllInBtn) AllInBtn->SetIsEnabled(bEnabled);
	if (DieBtn) DieBtn->SetIsEnabled(bEnabled);
}

void USeotdaTempWidget::ClearLocalCardSelection()
{
	if (bSelected0 && Card0Select)
	{
		PlayAnimation(Card0Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	if (bSelected1 && Card1Select)
	{
		PlayAnimation(Card1Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	if (bSelected2 && Card2Select)
	{
		PlayAnimation(Card2Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}

	bSelected0 = false;
	bSelected1 = false;
	bSelected2 = false;
}

FString USeotdaTempWidget::BuildCardIdListString(const TArray<int32>& Ids) const
{
	TArray<FString> Parts;
	for (int32 Id : Ids)
	{
		Parts.Add(FString::FromInt(Id));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(",")) : TEXT("Empty");
}

void USeotdaTempWidget::RefreshOpponentSeats(AMainPlayerController* PC)
{
	if (!PC) return;

	const TArray<FSeotdaOpponentInfo>& Opponents = PC->SeotdaUiOpponents;
	const FSeotdaOpponentInfo* SeatInfos[SeotdaOpponentSeatCount] = {};

	// The server sends absolute table seats. Compacting the opponent array here used
	// to place cards in the wrong WBP slot, which could leave the revealed card under
	// a hidden/local seat panel until the final two-card submission refreshed the UI.
	for (const FSeotdaOpponentInfo& Opponent : Opponents)
	{
		if (Opponent.SeatIndex >= 0 &&
			Opponent.SeatIndex < SeotdaOpponentSeatCount &&
			SeatInfos[Opponent.SeatIndex] == nullptr)
		{
			SeatInfos[Opponent.SeatIndex] = &Opponent;
		}
	}

	// Opponents intentionally exclude this client. Reconstruct the missing table seat
	// so the locally selected public card is visible at the same time as everyone else's.
	FSeotdaOpponentInfo LocalInfo;
	if (AMainPlayerState* LocalPS = PC->GetPlayerState<AMainPlayerState>())
	{
		int32 LocalSeatIndex = INDEX_NONE;
		const int32 OccupiedSeatCount = FMath::Clamp(Opponents.Num() + 1, 1, SeotdaOpponentSeatCount);
		for (int32 SeatIndex = 0; SeatIndex < OccupiedSeatCount; ++SeatIndex)
		{
			if (SeatInfos[SeatIndex] == nullptr)
			{
				LocalSeatIndex = SeatIndex;
				break;
			}
		}

		if (LocalSeatIndex == INDEX_NONE)
		{
			for (int32 SeatIndex = 0; SeatIndex < SeotdaOpponentSeatCount; ++SeatIndex)
			{
				if (SeatInfos[SeatIndex] == nullptr)
				{
					LocalSeatIndex = SeatIndex;
					break;
				}
			}
		}

		if (LocalSeatIndex != INDEX_NONE)
		{
			LocalInfo.PlayerName = LocalPS->GetPlayerName();
			LocalInfo.SeatIndex = LocalSeatIndex;
			LocalInfo.BetMoney = PC->SeotdaUiMyBetMoney;
			LocalInfo.bFolded = PC->bSeotdaUiMyFolded;
			LocalInfo.bIsCurrentTurn = PC->bSeotdaUiMyTurn;

			if (LocalPS->HasRevealedCard())
			{
				LocalInfo.bHasRevealedCard = true;
				LocalInfo.RevealedCardID = LocalPS->GetRevealedCard().CardID;
			}
			else if (PC->bSeotdaUiMyRevealConfirmed && ConfirmedLocalRevealCardID != ECardID::None)
			{
				LocalInfo.bHasRevealedCard = true;
				LocalInfo.RevealedCardID = ConfirmedLocalRevealCardID;
			}

			SeatInfos[LocalSeatIndex] = &LocalInfo;
		}
	}

	// 폴드(다이)한 좌석은 별도 가림막 없이, 기존 위젯들 투명도를 낮춰서 표현한다.
	constexpr float FoldedOpacity = 0.35f;
	constexpr float NormalOpacity = 1.0f;

	for (int32 i = 0; i < SeotdaOpponentSeatCount; ++i)
	{
		if (SeatInfos[i] == nullptr)
		{
			// 상대가 좌석 수보다 적으면 배경까지 포함해 좌석 전체를 숨긴다.
			if (SeatBackgrounds[i]) SeatBackgrounds[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatNameTexts[i]) SeatNameTexts[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatChipTexts[i]) SeatChipTexts[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatTurnHighlights[i]) SeatTurnHighlights[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatCardImages[i]) SeatCardImages[i]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FSeotdaOpponentInfo& Info = *SeatInfos[i];
		const float SeatOpacity = Info.bFolded ? FoldedOpacity : NormalOpacity;

		if (SeatBackgrounds[i])
		{
			SeatBackgrounds[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
			SeatBackgrounds[i]->SetRenderOpacity(SeatOpacity);
		}
		if (SeatNameTexts[i])
		{
			SeatNameTexts[i]->SetVisibility(ESlateVisibility::Visible);
			SeatNameTexts[i]->SetText(FText::FromString(TruncateDisplayName(Info.PlayerName)));
			SeatNameTexts[i]->SetRenderOpacity(SeatOpacity);
		}
		if (SeatChipTexts[i])
		{
			SeatChipTexts[i]->SetVisibility(ESlateVisibility::Visible);
			SeatChipTexts[i]->SetText(Info.bAllIn
				? FText::FromString(FString::Printf(TEXT("ALL-IN (%d)"), Info.BetMoney))
				: FText::AsNumber(Info.BetMoney));
			SeatChipTexts[i]->SetRenderOpacity(SeatOpacity);
		}
		if (SeatTurnHighlights[i])
		{
			SeatTurnHighlights[i]->SetVisibility(Info.bIsCurrentTurn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			SeatTurnHighlights[i]->SetRenderOpacity(SeatOpacity);
		}
		if (SeatCardImages[i])
		{
			if (Info.bHasRevealedCard)
			{
				if (UTexture2D* Texture = CardTextures ? CardTextures->GetFront(Info.RevealedCardID) : nullptr)
				{
					SeatCardImages[i]->SetBrushFromTexture(Texture, true);
				}
				// 공개 상태는 이미 서버에서 즉시 복제된다. 기존 WBP의 카드 부모 패널도
				// 함께 열어 최종 2장 제출 전부터 공개 카드가 보이게 한다.
				EnsureWidgetHierarchyVisible(SeatCardImages[i], 16);
			}
			else
			{
				SeatCardImages[i]->SetVisibility(ESlateVisibility::Collapsed);
			}
			SeatCardImages[i]->SetRenderOpacity(SeatOpacity);
		}
	}
}

void USeotdaTempWidget::RefreshFromPlayerState()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
	if (!PS)
	{
		return;
	}

	const TArray<FOwnedCardInfo> Cards = PS->GetOwnedCards();
	ResetLocalRoundUiState(Cards);

	if (LastHandledRevealResultSerial != PC->SeotdaUiRevealResultSerial)
	{
		LastHandledRevealResultSerial = PC->SeotdaUiRevealResultSerial;
		bLocalRevealPending = false;
		if (PC->bSeotdaUiRevealAccepted)
		{
			if (PendingLocalRevealCardID != ECardID::None)
			{
				ConfirmedLocalRevealCardID = PendingLocalRevealCardID;
			}
			ClearLocalCardSelection();
		}
		else
		{
			ConfirmedLocalRevealCardID = ECardID::None;
		}
		PendingLocalRevealCardID = ECardID::None;
	}

	if (LastHandledSelectionResultSerial != PC->SeotdaUiSelectionResultSerial)
	{
		LastHandledSelectionResultSerial = PC->SeotdaUiSelectionResultSerial;
		bLocalSelectionPending = false;
	}

	if (PC->bSeotdaUiMyRevealConfirmed)
	{
		bLocalRevealPending = false;
		if (PS->HasRevealedCard())
		{
			ConfirmedLocalRevealCardID = PS->GetRevealedCard().CardID;
		}
	}

	if (PC->bSeotdaUiMySubmitted)
	{
		bLocalRevealPending = false;
		bLocalSelectionPending = false;
	}

	RefreshOpponentSeats(PC);

	if (LobbyBtn)
	{
		const bool bShowLobbyButton = PC->bSeotdaUiMatchEnded;
		LobbyBtn->SetVisibility(bShowLobbyButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		LobbyBtn->SetIsEnabled(bShowLobbyButton);
	}

	if (PC->bSeotdaUiMyRevealConfirmed != bLastKnownRevealConfirmed)
	{
		ClearLocalCardSelection();
		bLastKnownRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	}

	if (InfoTxt)
	{
		EnsureWidgetHierarchyVisible(InfoTxt, 2);
		InfoTxt->SetText(FText::FromString(FString::Printf(
			TEXT("골드 %d"),
			PS->CurPlayerData.HoldingGold)));
	}

	UpdateCardButtonText(Card0Txt, Card0Img, 0, Cards);
	UpdateCardButtonText(Card1Txt, Card1Img, 1, Cards);
	UpdateCardButtonText(Card2Txt, Card2Img, 2, Cards);

	const bool bHasThreeCards = Cards.Num() >= 3;
	const bool bRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	const bool bSelectionSubmitted = PC->bSeotdaUiMySubmitted;
	const bool bAnySelectionPending = bLocalRevealPending || bLocalSelectionPending;
	const bool bSelectionLocked = bSelectionSubmitted || bAnySelectionPending;
	const bool bCanSelect = bHasThreeCards && !bSelectionLocked;
	const int32 RequiredSelectionCount = bRevealConfirmed ? 2 : 1;
	const bool bCanSubmit = bCanSelect && GetSelectedCount() == RequiredSelectionCount;

	SetCardSelectionButtonsEnabled(bCanSelect);

	UButton* CardBtns[] = { Card0Btn, Card1Btn, Card2Btn };
	UImage* CardImgs[] = { Card0Img, Card1Img, Card2Img };
	UTextBlock* CardTxts[] = { Card0Txt, Card1Txt, Card2Txt };
	const bool bSelectedFlags[] = { bSelected0, bSelected1, bSelected2 };
	for (int32 i = 0; i < 3; ++i)
	{
		float TargetOpacity = 0.5f;

		if (bSelectionLocked || !bHasThreeCards)
		{
			// 제출 대기 중/제출 완료/3장 미만 시 모든 카드 불꺼짐 (35% opacity)
			TargetOpacity = 0.35f;
		}
		else if (bSelectedFlags[i])
		{
			// 선택된 카드는 불 켜짐 (100% opacity)
			TargetOpacity = 1.0f;
		}
		else
		{
			// 라운드 시작 직후(미선택) 및 미선택 카드는 기본 불꺼짐 (50% opacity)
			TargetOpacity = 0.5f;
		}

		if (CardBtns[i]) CardBtns[i]->SetRenderOpacity(TargetOpacity);
		if (CardImgs[i]) CardImgs[i]->SetRenderOpacity(TargetOpacity);
		if (CardTxts[i]) CardTxts[i]->SetRenderOpacity(TargetOpacity);
	}

	if (SubmitBtn)
	{
		SubmitBtn->SetIsEnabled(bCanSubmit);
	}

	SetBetButtonsEnabled(bSelectionSubmitted && PC->bSeotdaUiBettingActive && PC->bSeotdaUiMyTurn && !PC->bSeotdaUiMyFolded && !PC->bSeotdaUiRoundResolved);

	if (RoundTxt)
	{
		RoundTxt->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UBorder* RoundIcon = Cast<UBorder>(GetWidgetFromName(TEXT("Border_209"))))
	{
		if (UTexture2D* Texture = GetRoundIconTexture(PC->SeotdaUiRound))
		{
			RoundIcon->SetBrushFromTexture(Texture);
			RoundIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			RoundIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (PotTxt)
	{
		PotTxt->SetText(FText::FromString(FString::Printf(TEXT("팟 %d"), PC->SeotdaUiPot)));
	}
	if (TurnTxt)
	{
		const FString DisplayTurnName = TruncateDisplayName(PC->SeotdaUiCurrentTurnPlayerName);
		TurnTxt->SetText(FText::FromString(FString::Printf(TEXT("현재 턴: %s"), *DisplayTurnName)));
	}

}

void USeotdaTempWidget::UpdateCardButtonText(UTextBlock* TargetText, UImage* CardImage, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards)
{
	if (!TargetText)
	{
		return;
	}

	if (!Cards.IsValidIndex(CardIndex))
	{
		TargetText->SetText(FText::GetEmpty());
		if (CardImage)
		{
			CardImage->SetBrush(FSlateNoResource());
		}
		return;
	}

	const FOwnedCardInfo& CardInfo = Cards[CardIndex];
	TargetText->SetText(FText::FromString(FormatCardShortLabel(CardInfo.CardID)));

	// 카드 이미지 업데이트
	if (CardImage && CardTextures)
	{
		if (UTexture2D* Texture = CardTextures->GetFront(CardInfo.CardID))
		{
			CardImage->SetBrush(FSlateImageBrush(Texture, FVector2D(256.0f, 256.0f)));
		}
	}
}

int32 USeotdaTempWidget::GetSelectedCount() const
{
	int32 Count = 0;
	if (bSelected0) Count++;
	if (bSelected1) Count++;
	if (bSelected2) Count++;
	return Count;
}

void USeotdaTempWidget::ToggleCardSelection(int32 CardIndex)
{
	const AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	if (bLocalRevealPending || bLocalSelectionPending || PC->bSeotdaUiMySubmitted)
	{
		return;
	}

	bool* Target = nullptr;
	if (CardIndex == 0) Target = &bSelected0;
	if (CardIndex == 1) Target = &bSelected1;
	if (CardIndex == 2) Target = &bSelected2;

	if (!Target)
	{
		return;
	}

	if (*Target)
	{
		if (CardIndex == 0 && Card0Select)
		{
			PlayAnimation(Card0Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex == 1 && Card1Select)
		{
			PlayAnimation(Card1Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex == 2 && Card2Select)
		{
			PlayAnimation(Card2Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		*Target = false;
		RefreshFromPlayerState();
		return;
	}

	if (PC->bSeotdaUiMyRevealConfirmed)
	{
		if (GetSelectedCount() >= 2)
		{
			RefreshFromPlayerState();
			return;
		}

		*Target = true;
	}
	else
	{
		if (CardIndex != 0 && bSelected0 && Card0Select)
		{
			PlayAnimation(Card0Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex != 1 && bSelected1 && Card1Select)
		{
			PlayAnimation(Card1Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex != 2 && bSelected2 && Card2Select)
		{
			PlayAnimation(Card2Select, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}

		bSelected0 = CardIndex == 0;
		bSelected1 = CardIndex == 1;
		bSelected2 = CardIndex == 2;
	}

	if (CardIndex == 0 && Card0Select)
	{
		PlayAnimation(Card0Select);
	}
	if (CardIndex == 1 && Card1Select)
	{
		PlayAnimation(Card1Select);
	}
	if (CardIndex == 2 && Card2Select)
	{
		PlayAnimation(Card2Select);
	}

	RefreshFromPlayerState();
}

void USeotdaTempWidget::SubmitSelection()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	if (bLocalRevealPending || bLocalSelectionPending || PC->bSeotdaUiMySubmitted)
	{
		return;
	}

	const bool bRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	const int32 RequiredSelectionCount = bRevealConfirmed ? 2 : 1;
	if (GetSelectedCount() != RequiredSelectionCount)
	{
		return;
	}

	if (!bRevealConfirmed)
	{
		AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
		const TArray<FOwnedCardInfo> Cards = PS ? PS->GetOwnedCards() : TArray<FOwnedCardInfo>();
		const int32 SelectedCardIndex = bSelected0 ? 0 : (bSelected1 ? 1 : (bSelected2 ? 2 : INDEX_NONE));
		if (!Cards.IsValidIndex(SelectedCardIndex))
		{
			return;
		}

		PendingLocalRevealCardID = Cards[SelectedCardIndex].CardID;
	}

	bLocalRevealPending = !bRevealConfirmed;
	bLocalSelectionPending = bRevealConfirmed;
	SetCardSelectionButtonsEnabled(false);
	if (SubmitBtn)
	{
		SubmitBtn->SetIsEnabled(false);
	}

	if (bRevealConfirmed)
	{
		PC->Server_SubmitSeotdaSelection(bSelected0, bSelected1, bSelected2);
	}
	else
	{
		PC->Server_RevealSeotdaCard(bSelected0, bSelected1, bSelected2);
	}

	RefreshFromPlayerState();
}

void USeotdaTempWidget::RequestBetAction(EBettingAction Action)
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	if (!PC->bSeotdaUiMySubmitted)
	{
		return;
	}

	if (GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastBetActionTimeSeconds < 0.5)
		{
			return;
		}
		LastBetActionTimeSeconds = Now;
	}

	PC->Server_RequestSeotdaBetAction(Action);
}

void USeotdaTempWidget::OnCard0Clicked()
{
	ToggleCardSelection(0);
}

void USeotdaTempWidget::OnCard0Hovered()
{
	if (!bSelected0 && !bLocalRevealPending && !bLocalSelectionPending)
	{
		if (Card0Btn) Card0Btn->SetRenderOpacity(1.0f);
		if (Card0Img) Card0Img->SetRenderOpacity(1.0f);
		if (Card0Txt) Card0Txt->SetRenderOpacity(1.0f);
	}
	if (Card0Hover)
	{
		PlayAnimation(Card0Hover);
	}
}

void USeotdaTempWidget::OnCard0Unhovered()
{
	if (Card0Hover)
	{
		PlayAnimation(Card0Hover, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	RefreshFromPlayerState();
}

void USeotdaTempWidget::OnCard1Clicked()
{
	ToggleCardSelection(1);
}

void USeotdaTempWidget::OnCard1Hovered()
{
	if (!bSelected1 && !bLocalRevealPending && !bLocalSelectionPending)
	{
		if (Card1Btn) Card1Btn->SetRenderOpacity(1.0f);
		if (Card1Img) Card1Img->SetRenderOpacity(1.0f);
		if (Card1Txt) Card1Txt->SetRenderOpacity(1.0f);
	}
	if (Card1Hover)
	{
		PlayAnimation(Card1Hover);
	}
}

void USeotdaTempWidget::OnCard1Unhovered()
{
	if (Card1Hover)
	{
		PlayAnimation(Card1Hover, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	RefreshFromPlayerState();
}

void USeotdaTempWidget::OnCard2Clicked()
{
	ToggleCardSelection(2);
}

void USeotdaTempWidget::OnCard2Hovered()
{
	if (!bSelected2 && !bLocalRevealPending && !bLocalSelectionPending)
	{
		if (Card2Btn) Card2Btn->SetRenderOpacity(1.0f);
		if (Card2Img) Card2Img->SetRenderOpacity(1.0f);
		if (Card2Txt) Card2Txt->SetRenderOpacity(1.0f);
	}
	if (Card2Hover)
	{
		PlayAnimation(Card2Hover);
	}
}

void USeotdaTempWidget::OnCard2Unhovered()
{
	if (Card2Hover)
	{
		PlayAnimation(Card2Hover, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	RefreshFromPlayerState();
}

void USeotdaTempWidget::OnSubmitClicked()
{
	SubmitSelection();
}

void USeotdaTempWidget::OnCheckClicked()
{
	RequestBetAction(EBettingAction::Check);
}

void USeotdaTempWidget::OnCallClicked()
{
	RequestBetAction(EBettingAction::Call);
}

void USeotdaTempWidget::OnQuarterClicked()
{
	RequestBetAction(EBettingAction::Quarter);
}

void USeotdaTempWidget::OnHalfClicked()
{
	RequestBetAction(EBettingAction::Half);
}

void USeotdaTempWidget::OnDdadangClicked()
{
	RequestBetAction(EBettingAction::Ddadang);
}

void USeotdaTempWidget::OnPpingClicked()
{
	RequestBetAction(EBettingAction::Pping);
}

void USeotdaTempWidget::OnAllInClicked()
{
	RequestBetAction(EBettingAction::AllIn);
}

void USeotdaTempWidget::OnDieClicked()
{
	RequestBetAction(EBettingAction::Die);
}

void USeotdaTempWidget::OnLobbyClicked()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	PC->ReturnToLobbyFromMatchEnd();
}
