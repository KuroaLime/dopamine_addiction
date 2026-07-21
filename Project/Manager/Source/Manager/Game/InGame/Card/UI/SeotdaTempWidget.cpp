#include "Game/InGame/Card/UI/SeotdaTempWidget.h"
#include "Game/InGame/Card/Data/CardTextureSet.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Slate/SlateBrushAsset.h"

void USeotdaTempWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindWidgetsByName();
	BindButtonEvents();
	if (LobbyButton)
	{
		LobbyButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnLobbyClicked);
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
	bSelected0 = false;
	bSelected1 = false;
	bSelected2 = false;
	bLocalSelectionSubmitted = false;
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
			ResultText->SetText(FText::FromString(TEXT("Result: New card set detected. Select 2 cards.")));
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

FString USeotdaTempWidget::BuildCardIdListString(const TArray<int32>& Ids) const
{
	TArray<FString> Parts;
	for (int32 Id : Ids)
	{
		Parts.Add(FString::FromInt(Id));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(",")) : TEXT("Empty");
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

	if (LobbyButton)
	{
		const bool bShowLobbyButton = PC->bSeotdaUiMatchEnded;
		LobbyButton->SetVisibility(bShowLobbyButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		LobbyButton->SetIsEnabled(bShowLobbyButton);
	}

	if (ResultText)
	{
		if (!PC->SeotdaUiLastResultText.IsEmpty())
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

	const bool bHasThreeCards = Cards.Num() >= 3;
	const bool bCanSelect = bHasThreeCards && !bLocalSelectionSubmitted;
	const bool bCanSubmit = bCanSelect && GetSelectedCount() == 2;

	SetCardSelectionButtonsEnabled(bCanSelect);

	if (SubmitButton)
	{
		SubmitButton->SetIsEnabled(bCanSubmit);
	}

	if (SubmitText)
	{
		SubmitText->SetText(FText::FromString(bLocalSelectionSubmitted ? TEXT("Submitted / Waiting") : TEXT("Submit Selection")));
	}

	SetBetButtonsEnabled(bLocalSelectionSubmitted && PC->bSeotdaUiBettingActive && PC->bSeotdaUiMyTurn && !PC->bSeotdaUiMyFolded && !PC->bSeotdaUiRoundResolved);

	if (StatusText)
	{
		if (bLocalSelectionSubmitted)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Submitted. Wait for betting turn. Server validates turn.")));
		}
		else if (!bHasThreeCards)
		{
			StatusText->SetText(FText::FromString(TEXT("Status: Need 3 cards before submit.")));
		}
		else
		{
			StatusText->SetText(FText::FromString(FString::Printf(
				TEXT("Status: Selected %d/2. Select exactly 2 cards."),
				GetSelectedCount()
			)));
		}
	}

	if (BetInfoText)
	{
		BetInfoText->SetText(FText::FromString(FString::Printf(
			TEXT("Bet: Round=%d | Pot=%d | CurrentBet=%d | MyBet=%d | NeedCall=%d | Turn=%s"),
			PC->SeotdaUiRound,
			PC->SeotdaUiPot,
			PC->SeotdaUiCurrentBet,
			PC->SeotdaUiMyBetMoney,
			PC->SeotdaUiNeedCall,
			*PC->SeotdaUiCurrentTurnPlayerName
		)));
	}

	if (StatusText && bLocalSelectionSubmitted)
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

	const FString SelectedPrefix = bSelected ? TEXT("[SELECTED] ") : TEXT("");

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
	if (bLocalSelectionSubmitted)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Already submitted this round.")));
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
		*Target = false;
		RefreshFromPlayerState();
		return;
	}

	if (GetSelectedCount() >= 2)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Already selected 2 cards. Unselect one first.")));
		}
		return;
	}

	*Target = true;
	RefreshFromPlayerState();
}

void USeotdaTempWidget::SubmitSelection()
{
	AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	if (bLocalSelectionSubmitted)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Submit ignored. Already submitted this round.")));
		}
		return;
	}

	if (GetSelectedCount() != 2)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Select exactly 2 cards before submit.")));
		}
		return;
	}

	bLocalSelectionSubmitted = true;
	SetCardSelectionButtonsEnabled(false);
	if (SubmitButton)
	{
		SubmitButton->SetIsEnabled(false);
	}

	PC->Server_SubmitSeotdaSelection(bSelected0, bSelected1, bSelected2);

	if (ResultText)
	{
		ResultText->SetText(FText::FromString(FString::Printf(
			TEXT("Result: Selection submitted [%d,%d,%d]. Waiting for all players / betting turn."),
			bSelected0 ? 1 : 0,
			bSelected1 ? 1 : 0,
			bSelected2 ? 1 : 0
		)));
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

	if (!bLocalSelectionSubmitted)
	{
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(TEXT("Result: Submit 2 cards before betting.")));
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
	if (bSelected0)
	{
		// 선택 해제 - SelectAnim 역재생
		if (Card0_SelectAnim)
		{
			PlayAnimation(Card0_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
	}
	else
	{
		// 선택 - SelectAnim 재생
		if (Card0_SelectAnim)
		{
			PlayAnimation(Card0_SelectAnim);
		}
	}
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
	if (bSelected1)
	{
		// 선택 해제 - SelectAnim 역재생
		if (Card1_SelectAnim)
		{
			PlayAnimation(Card1_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
	}
	else
	{
		// 선택 - SelectAnim 재생
		if (Card1_SelectAnim)
		{
			PlayAnimation(Card1_SelectAnim);
		}
	}
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
	if (bSelected2)
	{
		// 선택 해제 - SelectAnim 역재생
		if (Card2_SelectAnim)
		{
			PlayAnimation(Card2_SelectAnim, 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
	}
	else
	{
		// 선택 - SelectAnim 재생
		if (Card2_SelectAnim)
		{
			PlayAnimation(Card2_SelectAnim);
		}
	}
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
