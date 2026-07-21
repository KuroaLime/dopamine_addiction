#include "Game/InGame/Card/UI/SeotdaTempWidget.h"
#include "Game/InGame/Card/Data/CardTextureSet.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "GameFramework/GameStateBase.h"
#include "Slate/SlateBrushAsset.h"

void USeotdaTempWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!CardTextures)
	{
		CardTextures = UCardTextureSet::LoadDefault();
	}

	BindWidgetsByName();
	EnsurePublicCardsPanel();
	BindButtonEvents();
	if (LobbyButton)
	{
		LobbyButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnLobbyClicked);
	}
	if (const AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		LastHandledRevealResultSerial = PC->SeotdaUiRevealResultSerial;
		LastHandledSelectionResultSerial = PC->SeotdaUiSelectionResultSerial;
	}
	RefreshFromPlayerState();

	UE_LOG(LogTemp, Warning, TEXT("[CL] SeotdaTempWidget NativeConstruct Root=%s RootBox=%s"),
		*GetNameSafe(RootBorder),
		*GetNameSafe(RootBox));
}

void USeotdaTempWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshFromPlayerState();
}

void USeotdaTempWidget::BindWidgetsByName()
{
	// Root Layout
	BindWidgetByName(RootBorder, TEXT("Root_Border"));
	BindWidgetByName(RootBox, TEXT("Root_Box"));

	// Info Texts
	BindWidgetByName(TitleText, TEXT("Title_Text"));
	BindWidgetByName(CardInfoText, TEXT("Card_Info_Text"));
	BindWidgetByName(StatusText, TEXT("Status_Text"));
	BindWidgetByName(BetInfoText, TEXT("Bet_Info_Text"));
	BindWidgetByName(ResultText, TEXT("Result_Text"));
	BindWidgetByName(PublicCardsTitleText, TEXT("Public_Cards_Title"));
	BindWidgetByName(PublicCardsBox, TEXT("Public_Cards_Box"));

	// Card Selection UI
	BindWidgetByName(CardButton0, TEXT("Card_Button_0"));
	BindWidgetByName(CardButton1, TEXT("Card_Button_1"));
	BindWidgetByName(CardButton2, TEXT("Card_Button_2"));
	BindWidgetByName(CardText0, TEXT("Card_Text_0"));
	BindWidgetByName(CardText1, TEXT("Card_Text_1"));
	BindWidgetByName(CardText2, TEXT("Card_Text_2"));
	BindWidgetByName(CardImage0, TEXT("Card_Image_0"));
	BindWidgetByName(CardImage1, TEXT("Card_Image_1"));
	BindWidgetByName(CardImage2, TEXT("Card_Image_2"));

	// Submit Button
	BindWidgetByName(SubmitButton, TEXT("Submit_Button"));
	BindWidgetByName(SubmitText, TEXT("Submit_Text"));

	// Betting Buttons
	BindWidgetByName(CheckButton, TEXT("Check_Button"));
	BindWidgetByName(CheckText, TEXT("Check_Text"));
	BindWidgetByName(CallButton, TEXT("Call_Button"));
	BindWidgetByName(CallText, TEXT("Call_Text"));
	BindWidgetByName(QuarterButton, TEXT("Quarter_Button"));
	BindWidgetByName(QuarterText, TEXT("Quarter_Text"));
	BindWidgetByName(HalfButton, TEXT("Half_Button"));
	BindWidgetByName(HalfText, TEXT("Half_Text"));
	BindWidgetByName(DdadangButton, TEXT("Ddadang_Button"));
	BindWidgetByName(DdadangText, TEXT("Ddadang_Text"));
	BindWidgetByName(PpingButton, TEXT("Pping_Button"));
	BindWidgetByName(PpingText, TEXT("Pping_Text"));
	BindWidgetByName(AllInButton, TEXT("AllIn_Button"));
	BindWidgetByName(AllInText, TEXT("AllIn_Text"));
	BindWidgetByName(DieButton, TEXT("Die_Button"));
	BindWidgetByName(DieText, TEXT("Die_Text"));

	// Lobby Button
	BindWidgetByName(LobbyButton, TEXT("Lobby_Button"));
	BindWidgetByName(LobbyText, TEXT("Lobby_Text"));

	// Opponent Seats
	BindWidgetByName(Seat0_NameText, TEXT("Seat0_NameText"));
	BindWidgetByName(Seat1_NameText, TEXT("Seat1_NameText"));
	BindWidgetByName(Seat2_NameText, TEXT("Seat2_NameText"));
	BindWidgetByName(Seat3_NameText, TEXT("Seat3_NameText"));
	BindWidgetByName(Seat0_ChipText, TEXT("Seat0_ChipText"));
	BindWidgetByName(Seat1_ChipText, TEXT("Seat1_ChipText"));
	BindWidgetByName(Seat2_ChipText, TEXT("Seat2_ChipText"));
	BindWidgetByName(Seat3_ChipText, TEXT("Seat3_ChipText"));
	BindWidgetByName(Seat0_FoldedOverlay, TEXT("Seat0_FoldedOverlay"));
	BindWidgetByName(Seat1_FoldedOverlay, TEXT("Seat1_FoldedOverlay"));
	BindWidgetByName(Seat2_FoldedOverlay, TEXT("Seat2_FoldedOverlay"));
	BindWidgetByName(Seat3_FoldedOverlay, TEXT("Seat3_FoldedOverlay"));
	BindWidgetByName(Seat0_TurnHighlight, TEXT("Seat0_TurnHighlight"));
	BindWidgetByName(Seat1_TurnHighlight, TEXT("Seat1_TurnHighlight"));
	BindWidgetByName(Seat2_TurnHighlight, TEXT("Seat2_TurnHighlight"));
	BindWidgetByName(Seat3_TurnHighlight, TEXT("Seat3_TurnHighlight"));

	SeatNameTexts[0] = Seat0_NameText;
	SeatNameTexts[1] = Seat1_NameText;
	SeatNameTexts[2] = Seat2_NameText;
	SeatNameTexts[3] = Seat3_NameText;

	SeatChipTexts[0] = Seat0_ChipText;
	SeatChipTexts[1] = Seat1_ChipText;
	SeatChipTexts[2] = Seat2_ChipText;
	SeatChipTexts[3] = Seat3_ChipText;

	SeatFoldedOverlays[0] = Seat0_FoldedOverlay;
	SeatFoldedOverlays[1] = Seat1_FoldedOverlay;
	SeatFoldedOverlays[2] = Seat2_FoldedOverlay;
	SeatFoldedOverlays[3] = Seat3_FoldedOverlay;

	SeatTurnHighlights[0] = Seat0_TurnHighlight;
	SeatTurnHighlights[1] = Seat1_TurnHighlight;
	SeatTurnHighlights[2] = Seat2_TurnHighlight;
	SeatTurnHighlights[3] = Seat3_TurnHighlight;
}

void USeotdaTempWidget::BindButtonEvents()
{
	// Card Selection Events
	if (CardButton0)
	{
		CardButton0->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
		CardButton0->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
		CardButton0->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Hovered);
		CardButton0->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard0Hovered);
		CardButton0->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Unhovered);
		CardButton0->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard0Unhovered);
	}
	if (CardButton1)
	{
		CardButton1->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
		CardButton1->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
		CardButton1->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Hovered);
		CardButton1->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard1Hovered);
		CardButton1->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Unhovered);
		CardButton1->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard1Unhovered);
	}
	if (CardButton2)
	{
		CardButton2->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
		CardButton2->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
		CardButton2->OnHovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Hovered);
		CardButton2->OnHovered.AddDynamic(this, &USeotdaTempWidget::OnCard2Hovered);
		CardButton2->OnUnhovered.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Unhovered);
		CardButton2->OnUnhovered.AddDynamic(this, &USeotdaTempWidget::OnCard2Unhovered);
	}

	// Submit Button
	if (SubmitButton)
	{
		SubmitButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
		SubmitButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
	}

	// Betting Button Events
	if (CheckButton)
	{
		CheckButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCheckClicked);
		CheckButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCheckClicked);
	}
	if (CallButton)
	{
		CallButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCallClicked);
		CallButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCallClicked);
	}
	if (QuarterButton)
	{
		QuarterButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnQuarterClicked);
		QuarterButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnQuarterClicked);
	}
	if (HalfButton)
	{
		HalfButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnHalfClicked);
		HalfButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnHalfClicked);
	}
	if (DdadangButton)
	{
		DdadangButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnDdadangClicked);
		DdadangButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnDdadangClicked);
	}
	if (PpingButton)
	{
		PpingButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnPpingClicked);
		PpingButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnPpingClicked);
	}
	if (AllInButton)
	{
		AllInButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnAllInClicked);
		AllInButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnAllInClicked);
	}
	if (DieButton)
	{
		DieButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnDieClicked);
		DieButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnDieClicked);
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
	bLastKnownRevealConfirmed = false;
	LocalSelectionFeedback.Empty();
	LastBetActionTimeSeconds = -1000.0;

	SetCardSelectionButtonsEnabled(Cards.Num() >= 3);
	SetBetButtonsEnabled(false);

	if (SubmitButton)
	{
		SubmitButton->SetIsEnabled(false);
	}

	if (ResultText)
	{
		if (Cards.Num() <= 0)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Cards cleared. Waiting for next round cards.")));
		}
		else
		{
			ResultText->SetText(FText::FromString(
				TEXT("Result: New card set detected. Choose 1 card to reveal.")));
		}
	}
}

void USeotdaTempWidget::SetCardSelectionButtonsEnabled(bool bEnabled)
{
	if (CardButton0) CardButton0->SetIsEnabled(bEnabled);
	if (CardButton1) CardButton1->SetIsEnabled(bEnabled);
	if (CardButton2) CardButton2->SetIsEnabled(bEnabled);
}

void USeotdaTempWidget::SetBetButtonsEnabled(bool bEnabled)
{
	if (CheckButton) CheckButton->SetIsEnabled(bEnabled);
	if (CallButton) CallButton->SetIsEnabled(bEnabled);
	if (QuarterButton) QuarterButton->SetIsEnabled(bEnabled);
	if (HalfButton) HalfButton->SetIsEnabled(bEnabled);
	if (DdadangButton) DdadangButton->SetIsEnabled(bEnabled);
	if (PpingButton) PpingButton->SetIsEnabled(bEnabled);
	if (AllInButton) AllInButton->SetIsEnabled(bEnabled);
	if (DieButton) DieButton->SetIsEnabled(bEnabled);
}

void USeotdaTempWidget::ClearLocalCardSelection()
{
	if (bSelected0 && Card0_SelectAnim)
	{
		PlayAnimation(Card0_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	if (bSelected1 && Card1_SelectAnim)
	{
		PlayAnimation(Card1_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
	if (bSelected2 && Card2_SelectAnim)
	{
		PlayAnimation(Card2_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}

	bSelected0 = false;
	bSelected1 = false;
	bSelected2 = false;
}

void USeotdaTempWidget::EnsurePublicCardsPanel()
{
	if (!RootBox || !WidgetTree)
	{
		return;
	}

	if (!PublicCardsTitleText)
	{
		PublicCardsTitleText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Runtime_Public_Cards_Title"));
		PublicCardsTitleText->SetText(FText::FromString(TEXT("Public Cards")));
		RootBox->AddChildToVerticalBox(PublicCardsTitleText);
	}

	if (!PublicCardsBox)
	{
		PublicCardsBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("Runtime_Public_Cards_Box"));
		RootBox->AddChildToVerticalBox(PublicCardsBox);
	}
}

void USeotdaTempWidget::RefreshPublicCardVisuals()
{
	EnsurePublicCardsPanel();
	if (!PublicCardsBox || !WidgetTree)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	TArray<FString> SignatureParts;
	for (APlayerState* BasePlayerState : GameState->PlayerArray)
	{
		const AMainPlayerState* PlayerState = Cast<AMainPlayerState>(BasePlayerState);
		if (!PlayerState)
		{
			continue;
		}

		const FOwnedCardInfo CardInfo = PlayerState->GetRevealedCard();
		SignatureParts.Add(FString::Printf(
			TEXT("%s:%d:%d"),
			*PlayerState->GetPlayerName(),
			CardInfo.CardInstanceId,
			static_cast<int32>(CardInfo.CardID)));
	}

	const FString Signature = FString::Join(SignatureParts, TEXT("|"));
	if (Signature == LastPublicCardVisualSignature)
	{
		return;
	}
	LastPublicCardVisualSignature = Signature;

	PublicCardsBox->ClearChildren();
	for (APlayerState* BasePlayerState : GameState->PlayerArray)
	{
		const AMainPlayerState* PlayerState = Cast<AMainPlayerState>(BasePlayerState);
		if (!PlayerState)
		{
			continue;
		}

		UVerticalBox* PlayerColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>();
		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		UImage* PublicCardImage = WidgetTree->ConstructWidget<UImage>();
		UTextBlock* CardStatusText = WidgetTree->ConstructWidget<UTextBlock>();
		if (!PlayerColumn || !PlayerNameText || !CardSizeBox || !PublicCardImage || !CardStatusText)
		{
			continue;
		}

		const FString PlayerName = PlayerState->GetPlayerName().IsEmpty()
			? GetNameSafe(PlayerState)
			: PlayerState->GetPlayerName();
		PlayerNameText->SetText(FText::FromString(PlayerName));

		CardSizeBox->SetWidthOverride(96.0f);
		CardSizeBox->SetHeightOverride(140.0f);
		CardSizeBox->AddChild(PublicCardImage);

		if (PlayerState->HasRevealedCard())
		{
			const FOwnedCardInfo CardInfo = PlayerState->GetRevealedCard();
			if (UTexture2D* Texture = CardTextures
				? CardTextures->GetFront(CardInfo.CardID)
				: nullptr)
			{
				PublicCardImage->SetBrushFromTexture(Texture, true);
			}
			CardStatusText->SetText(FText::FromString(FString::Printf(
				TEXT("#%d %s"),
				CardInfo.CardInstanceId,
				*CardDebug::ToString(CardInfo.CardID))));
		}
		else
		{
			PublicCardImage->SetBrush(FSlateNoResource());
			CardStatusText->SetText(FText::FromString(TEXT("Waiting")));
		}

		PlayerColumn->AddChildToVerticalBox(PlayerNameText);
		PlayerColumn->AddChildToVerticalBox(CardSizeBox);
		PlayerColumn->AddChildToVerticalBox(CardStatusText);

		if (UHorizontalBoxSlot* PublicCardSlot = PublicCardsBox->AddChildToHorizontalBox(PlayerColumn))
		{
			PublicCardSlot->SetPadding(FMargin(6.0f, 2.0f));
			PublicCardSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
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

FString USeotdaTempWidget::BuildPublicCardSummary() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return TEXT("Public Cards: Waiting for game state");
	}

	TArray<FString> Parts;
	for (APlayerState* BasePlayerState : GameState->PlayerArray)
	{
		const AMainPlayerState* PlayerState = Cast<AMainPlayerState>(BasePlayerState);
		if (!PlayerState)
		{
			continue;
		}

		const FString PlayerName = PlayerState->GetPlayerName().IsEmpty()
			? GetNameSafe(PlayerState)
			: PlayerState->GetPlayerName();
		if (PlayerState->HasRevealedCard())
		{
			const FOwnedCardInfo CardInfo = PlayerState->GetRevealedCard();
			Parts.Add(FString::Printf(
				TEXT("%s=#%d:%s"),
				*PlayerName,
				CardInfo.CardInstanceId,
				*CardDebug::ToString(CardInfo.CardID)));
		}
		else
		{
			Parts.Add(FString::Printf(TEXT("%s=Waiting"), *PlayerName));
		}
	}

	return Parts.Num() > 0
		? FString::Printf(TEXT("Public Cards: %s"), *FString::Join(Parts, TEXT(" | ")))
		: TEXT("Public Cards: No players");
}

void USeotdaTempWidget::RefreshOpponentSeats(AMainPlayerController* PC)
{
	if (!PC) return;

	const TArray<FSeotdaOpponentInfo>& Opponents = PC->SeotdaUiOpponents;

	for (int32 i = 0; i < SeotdaOpponentSeatCount; ++i)
	{
		if (!Opponents.IsValidIndex(i))
		{
			// 상대가 좌석 수보다 적으면 남는 좌석은 통째로 숨긴다.
			if (SeatNameTexts[i]) SeatNameTexts[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatChipTexts[i]) SeatChipTexts[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatFoldedOverlays[i]) SeatFoldedOverlays[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (SeatTurnHighlights[i]) SeatTurnHighlights[i]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FSeotdaOpponentInfo& Info = Opponents[i];

		if (SeatNameTexts[i])
		{
			SeatNameTexts[i]->SetVisibility(ESlateVisibility::Visible);
			SeatNameTexts[i]->SetText(FText::FromString(Info.PlayerName));
		}
		if (SeatChipTexts[i])
		{
			SeatChipTexts[i]->SetVisibility(ESlateVisibility::Visible);
			SeatChipTexts[i]->SetText(Info.bAllIn
				? FText::FromString(FString::Printf(TEXT("ALL-IN (%d)"), Info.BetMoney))
				: FText::AsNumber(Info.BetMoney));
		}
		if (SeatFoldedOverlays[i])
		{
			SeatFoldedOverlays[i]->SetVisibility(Info.bFolded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (SeatTurnHighlights[i])
		{
			SeatTurnHighlights[i]->SetVisibility(Info.bIsCurrentTurn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void USeotdaTempWidget::RefreshFromPlayerState()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Missing owning player controller.")));
		}
		return;
	}

	RefreshOpponentSeats(PC);

	if (LastHandledRevealResultSerial != PC->SeotdaUiRevealResultSerial)
	{
		LastHandledRevealResultSerial = PC->SeotdaUiRevealResultSerial;
		bLocalRevealPending = false;
		if (PC->bSeotdaUiRevealAccepted)
		{
			ClearLocalCardSelection();
			LocalSelectionFeedback = TEXT("Result: Public card confirmed. Select exactly 2 cards for your final hand.");
		}
		else
		{
			LocalSelectionFeedback = FString::Printf(
				TEXT("Result: Server rejected the public card (%s). Select and retry."),
				*PC->SeotdaUiRevealResultReason);
		}
	}

	if (LastHandledSelectionResultSerial != PC->SeotdaUiSelectionResultSerial)
	{
		LastHandledSelectionResultSerial = PC->SeotdaUiSelectionResultSerial;
		bLocalSelectionPending = false;
		LocalSelectionFeedback = PC->bSeotdaUiSelectionAccepted
			? TEXT("Result: Server accepted the final hand. Waiting for all players / betting turn.")
			: FString::Printf(
				TEXT("Result: Server rejected the final hand (%s). Select and retry."),
				*PC->SeotdaUiSelectionResultReason);
	}

	if (PC->bSeotdaUiMyRevealConfirmed)
	{
		bLocalRevealPending = false;
	}

	if (PC->bSeotdaUiMySubmitted)
	{
		bLocalRevealPending = false;
		bLocalSelectionPending = false;
	}

	if (LobbyButton)
	{
		const bool bShowLobbyButton = PC->bSeotdaUiMatchEnded;
		LobbyButton->SetVisibility(bShowLobbyButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		LobbyButton->SetIsEnabled(bShowLobbyButton);
	}

	if (ResultText)
	{
		if (PC->bSeotdaUiMatchEnded || PC->bSeotdaUiRoundResolved)
		{
			ResultText->SetText(FText::FromString(PC->SeotdaUiLastResultText));
		}
		else if (!LocalSelectionFeedback.IsEmpty())
		{
			ResultText->SetText(FText::FromString(LocalSelectionFeedback));
		}
		else if (!PC->SeotdaUiLastResultText.IsEmpty())
		{
			ResultText->SetText(FText::FromString(PC->SeotdaUiLastResultText));
		}
		else
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Waiting...")));
		}
	}

	AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
	if (!PS)
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Missing player state.")));
		}
		return;
	}

	const TArray<FOwnedCardInfo> Cards = PS->GetOwnedCards();

	ResetLocalRoundUiState(Cards);
	if (PC->bSeotdaUiMyRevealConfirmed != bLastKnownRevealConfirmed)
	{
		ClearLocalCardSelection();
		bLastKnownRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	}

	if (CardInfoText)
	{
		CardInfoText->SetText(FText::FromString(FString::Printf(
			TEXT("My Cards: %d/3 | Money: %d"),
			Cards.Num(),
			PS->CurPlayerData.HoldingGold
		)));
	}

	UpdateCardButtonText(CardText0, CardImage0, 0, Cards);
	UpdateCardButtonText(CardText1, CardImage1, 1, Cards);
	UpdateCardButtonText(CardText2, CardImage2, 2, Cards);
	RefreshPublicCardVisuals();

	const bool bHasThreeCards = Cards.Num() >= 3;
	const bool bRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	const bool bSelectionSubmitted = PC->bSeotdaUiMySubmitted;
	const bool bAnySelectionPending = bLocalRevealPending || bLocalSelectionPending;
	const bool bSelectionLocked = bSelectionSubmitted || bAnySelectionPending;
	const bool bCanSelect = bHasThreeCards && !bSelectionLocked;
	const int32 RequiredSelectionCount = bRevealConfirmed ? 2 : 1;
	const bool bCanSubmit = bCanSelect && GetSelectedCount() == RequiredSelectionCount;

	SetCardSelectionButtonsEnabled(bCanSelect);

	if (SubmitButton)
	{
		SubmitButton->SetIsEnabled(bCanSubmit);
	}

	if (SubmitText)
	{
		const FString SubmitLabel = bLocalRevealPending
			? TEXT("Revealing...")
			: (bLocalSelectionPending
				? TEXT("Submitting...")
				: (bSelectionSubmitted
					? TEXT("Submitted / Waiting")
					: (bRevealConfirmed ? TEXT("Submit Final Hand") : TEXT("Reveal Public Card"))));
		SubmitText->SetText(FText::FromString(SubmitLabel));
	}

	if (TitleText)
	{
		const FString StageTitle = bSelectionSubmitted
			? TEXT("Seotda - Betting")
			: (bRevealConfirmed
				? TEXT("Seotda - Select Final 2-Card Hand")
				: TEXT("Seotda - Reveal 1 Public Card"));
		TitleText->SetText(FText::FromString(StageTitle));
	}

	SetBetButtonsEnabled(bSelectionSubmitted && PC->bSeotdaUiBettingActive && PC->bSeotdaUiMyTurn && !PC->bSeotdaUiMyFolded && !PC->bSeotdaUiRoundResolved);

	if (StatusText)
	{
		if (bLocalRevealPending)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Waiting for public-card approval.")));
		}
		else if (bLocalSelectionPending)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Waiting for final-hand approval.")));
		}
		else if (bSelectionSubmitted)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Submitted. Wait for betting turn. Server validates turn.")));
		}
		else if (!bHasThreeCards)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Need 3 cards before submit.")));
		}
		else
		{
			StatusText->SetText(FText::FromString(bRevealConfirmed
				? FString::Printf(
					TEXT("Status: Public card confirmed. Final hand selected %d/2."),
					GetSelectedCount())
				: FString::Printf(
					TEXT("Status: Public card selected %d/1."),
					GetSelectedCount())));
		}
	}

	if (BetInfoText)
	{
		BetInfoText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nBet: Round=%d | Pot=%d | CurrentBet=%d | MyBet=%d | NeedCall=%d | Turn=%s"),
			*BuildPublicCardSummary(),
			PC->SeotdaUiRound,
			PC->SeotdaUiPot,
			PC->SeotdaUiCurrentBet,
			PC->SeotdaUiMyBetMoney,
			PC->SeotdaUiNeedCall,
			*PC->SeotdaUiCurrentTurnPlayerName
		)));
	}

	if (StatusText && bSelectionSubmitted)
	{
		if (PC->bSeotdaUiRoundResolved)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Round resolved. Waiting for next phase.")));
		}
		else if (!PC->bSeotdaUiBettingActive)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Submitted. Waiting for all players to submit.")));
		}
		else if (PC->bSeotdaUiMyFolded)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Folded. Waiting for result.")));
		}
		else if (PC->bSeotdaUiMyTurn)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Your betting turn.")));
		}
		else
		{
			StatusText->SetText(FText::FromString(FString::Printf(
				TEXT("Status: Waiting for %s's betting turn."),
				*PC->SeotdaUiCurrentTurnPlayerName
			)));
		}
	}
}

void USeotdaTempWidget::UpdateCardButtonText(UTextBlock* TargetText, UImage* CardImage, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards)
{
	if (!TargetText)
	{
		return;
	}

	const bool bSelected =
		(CardIndex == 0 && bSelected0) ||
		(CardIndex == 1 && bSelected1) ||
		(CardIndex == 2 && bSelected2);

	const AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	const FString SelectedPrefix = bSelected
		? (PC && PC->bSeotdaUiMyRevealConfirmed ? TEXT("[HAND] ") : TEXT("[PUBLIC] "))
		: TEXT("");

	if (!Cards.IsValidIndex(CardIndex))
	{
		TargetText->SetText(FText::FromString(FString::Printf(
			TEXT("%d. Empty"),
			CardIndex + 1
		)));
		if (CardImage)
		{
			CardImage->SetBrush(FSlateNoResource());
		}
		return;
	}

	const FOwnedCardInfo& CardInfo = Cards[CardIndex];
	TargetText->SetText(FText::FromString(FString::Printf(
		TEXT("%d. %s#%d %s"),
		CardIndex + 1,
		*SelectedPrefix,
		CardInfo.CardInstanceId,
		*CardDebug::ToString(CardInfo.CardID)
	)));

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
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(
				(bLocalRevealPending || bLocalSelectionPending)
					? TEXT("Result: Waiting for server approval.")
					: TEXT("Result: Already submitted this round.")));
		}
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
		if (CardIndex == 0 && Card0_SelectAnim)
		{
			PlayAnimation(Card0_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex == 1 && Card1_SelectAnim)
		{
			PlayAnimation(Card1_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex == 2 && Card2_SelectAnim)
		{
			PlayAnimation(Card2_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		*Target = false;
		RefreshFromPlayerState();
		return;
	}

	if (PC->bSeotdaUiMyRevealConfirmed)
	{
		if (GetSelectedCount() >= 2)
		{
			LocalSelectionFeedback = TEXT("Result: Final hand can contain exactly 2 cards.");
			RefreshFromPlayerState();
			return;
		}

		*Target = true;
	}
	else
	{
		if (CardIndex != 0 && bSelected0 && Card0_SelectAnim)
		{
			PlayAnimation(Card0_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex != 1 && bSelected1 && Card1_SelectAnim)
		{
			PlayAnimation(Card1_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
		if (CardIndex != 2 && bSelected2 && Card2_SelectAnim)
		{
			PlayAnimation(Card2_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}

		bSelected0 = CardIndex == 0;
		bSelected1 = CardIndex == 1;
		bSelected2 = CardIndex == 2;
	}

	if (CardIndex == 0 && Card0_SelectAnim)
	{
		PlayAnimation(Card0_SelectAnim);
	}
	if (CardIndex == 1 && Card1_SelectAnim)
	{
		PlayAnimation(Card1_SelectAnim);
	}
	if (CardIndex == 2 && Card2_SelectAnim)
	{
		PlayAnimation(Card2_SelectAnim);
	}

	LocalSelectionFeedback.Empty();
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
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(
				(bLocalRevealPending || bLocalSelectionPending)
					? TEXT("Result: Submit ignored. Waiting for server approval.")
					: TEXT("Result: Submit ignored. Already submitted this round.")));
		}
		return;
	}

	const bool bRevealConfirmed = PC->bSeotdaUiMyRevealConfirmed;
	const int32 RequiredSelectionCount = bRevealConfirmed ? 2 : 1;
	if (GetSelectedCount() != RequiredSelectionCount)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(bRevealConfirmed
				? TEXT("Result: Select exactly 2 cards for the final hand.")
				: TEXT("Result: Select exactly 1 public card before reveal.")));
		}
		return;
	}

	bLocalRevealPending = !bRevealConfirmed;
	bLocalSelectionPending = bRevealConfirmed;
	LocalSelectionFeedback = bRevealConfirmed
		? TEXT("Result: Sending final hand to the server.")
		: TEXT("Result: Sending public card selection to the server.");
	SetCardSelectionButtonsEnabled(false);
	if (SubmitButton)
	{
		SubmitButton->SetIsEnabled(false);
	}

	if (bRevealConfirmed)
	{
		PC->Server_SubmitSeotdaSelection(bSelected0, bSelected1, bSelected2);
	}
	else
	{
		PC->Server_RevealSeotdaCard(bSelected0, bSelected1, bSelected2);
	}

	if (ResultText)
	{
		const int32 Selected0 = bSelected0 ? 1 : 0;
		const int32 Selected1 = bSelected1 ? 1 : 0;
		const int32 Selected2 = bSelected2 ? 1 : 0;
		const FString RequestMessage = bRevealConfirmed
			? FString::Printf(
				TEXT("Result: Final hand submitted [%d,%d,%d]. Waiting for server approval."),
				Selected0,
				Selected1,
				Selected2)
			: FString::Printf(
				TEXT("Result: Public card requested [%d,%d,%d]. Waiting for server approval."),
				Selected0,
				Selected1,
				Selected2);
		ResultText->SetText(FText::FromString(RequestMessage));
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
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Submit the final 2-card hand before betting.")));
		}
		return;
	}

	if (GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastBetActionTimeSeconds < 0.5)
		{
			if (ResultText)
			{
				ResultText->SetText(FText::FromString(TEXT("Result: Bet click ignored. Too fast.")));
			}
			return;
		}
		LastBetActionTimeSeconds = Now;
	}

	PC->Server_RequestSeotdaBetAction(Action);
	if (ResultText)
	{
		ResultText->SetText(FText::FromString(FString::Printf(
			TEXT("Result: Bet request sent. Action=%d. Server validates turn/state."),
			static_cast<int32>(Action)
		)));
	}
}

void USeotdaTempWidget::OnCard0Clicked()
{
	ToggleCardSelection(0);
}

void USeotdaTempWidget::OnCard0Hovered()
{
	if (Card0_HoverAnim)
	{
		PlayAnimation(Card0_HoverAnim);
	}
}

void USeotdaTempWidget::OnCard0Unhovered()
{
	// HoverAnim 역재생
	if (Card0_HoverAnim)
	{
		PlayAnimation(Card0_HoverAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
}

void USeotdaTempWidget::OnCard1Clicked()
{
	ToggleCardSelection(1);
}

void USeotdaTempWidget::OnCard1Hovered()
{
	if (Card1_HoverAnim)
	{
		PlayAnimation(Card1_HoverAnim);
	}
}

void USeotdaTempWidget::OnCard1Unhovered()
{
	// HoverAnim 역재생
	if (Card1_HoverAnim)
	{
		PlayAnimation(Card1_HoverAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
}

void USeotdaTempWidget::OnCard2Clicked()
{
	ToggleCardSelection(2);
}

void USeotdaTempWidget::OnCard2Hovered()
{
	if (Card2_HoverAnim)
	{
		PlayAnimation(Card2_HoverAnim);
	}
}

void USeotdaTempWidget::OnCard2Unhovered()
{
	// HoverAnim 역재생
	if (Card2_HoverAnim)
	{
		PlayAnimation(Card2_HoverAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
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
