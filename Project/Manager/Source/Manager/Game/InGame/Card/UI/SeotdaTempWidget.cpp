#include "Game/InGame/Card/UI/SeotdaTempWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"

TSharedRef<SWidget> USeotdaTempWidget::RebuildWidget()
{
BuildWidgetTree();
return Super::RebuildWidget();
}

void USeotdaTempWidget::NativeConstruct()
{
Super::NativeConstruct();

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

void USeotdaTempWidget::BuildWidgetTree()
{
if (!WidgetTree)
{
WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
}

RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SeotdaRootBorder"));
WidgetTree->RootWidget = RootBorder;

if (RootBorder)
{
RootBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.70f));
RootBorder->SetPadding(FMargin(16.0f));
}

RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SeotdaTempRootBox"));
if (RootBorder && RootBox)
{
RootBorder->SetContent(RootBox);
}

TitleText = MakeText(TEXT("TitleText"), TEXT("[SEOTDA CARD GAME]"), 24);
RootBox->AddChildToVerticalBox(TitleText);

CardInfoText = MakeText(TEXT("CardInfoText"), TEXT("My Cards: Loading..."), 16);
RootBox->AddChildToVerticalBox(CardInfoText);

UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardRow"));
RootBox->AddChildToVerticalBox(CardRow);

CardButton0 = MakeButton(TEXT("CardButton0"), CardText0, TEXT("Card 1"));
CardButton1 = MakeButton(TEXT("CardButton1"), CardText1, TEXT("Card 2"));
CardButton2 = MakeButton(TEXT("CardButton2"), CardText2, TEXT("Card 3"));

CardRow->AddChildToHorizontalBox(CardButton0);
CardRow->AddChildToHorizontalBox(CardButton1);
CardRow->AddChildToHorizontalBox(CardButton2);

SubmitButton = MakeButton(TEXT("SubmitButton"), SubmitText, TEXT("Submit Selection"));
RootBox->AddChildToVerticalBox(SubmitButton);

StatusText = MakeText(TEXT("StatusText"), TEXT("Select exactly 2 cards."), 16);
RootBox->AddChildToVerticalBox(StatusText);

BetInfoText = MakeText(TEXT("BetInfoText"), TEXT("Bet Info: Submit first."), 16);
RootBox->AddChildToVerticalBox(BetInfoText);

UHorizontalBox* BetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BetRow"));
RootBox->AddChildToVerticalBox(BetRow);

CheckButton = MakeButton(TEXT("CheckButton"), CheckText, TEXT("Check"));
CallButton = MakeButton(TEXT("CallButton"), CallText, TEXT("Call"));
HalfButton = MakeButton(TEXT("HalfButton"), HalfText, TEXT("Half"));
DieButton = MakeButton(TEXT("DieButton"), DieText, TEXT("Die"));

BetRow->AddChildToHorizontalBox(CheckButton);
BetRow->AddChildToHorizontalBox(CallButton);
BetRow->AddChildToHorizontalBox(HalfButton);
BetRow->AddChildToHorizontalBox(DieButton);

ResultText = MakeText(TEXT("ResultText"), TEXT("Result: None"), 16);
RootBox->AddChildToVerticalBox(ResultText);
    LobbyButton = MakeButton(TEXT("LobbyButton"), LobbyText, TEXT("로비로"));
    if (LobbyButton)
    {
        LobbyButton->SetVisibility(ESlateVisibility::Collapsed);
        RootBox->AddChildToVerticalBox(LobbyButton);
    }

SetBetButtonsEnabled(false);

UE_LOG(LogTemp, Warning, TEXT("[CL] SeotdaTempWidget BuildWidgetTree Root=%s RootBox=%s"),
*GetNameSafe(RootBorder),
*GetNameSafe(RootBox));
}

void USeotdaTempWidget::BindButtonEvents()
{
if (CardButton0)
{
CardButton0->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
CardButton0->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard0Clicked);
}
if (CardButton1)
{
CardButton1->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
CardButton1->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard1Clicked);
}
if (CardButton2)
{
CardButton2->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
CardButton2->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnCard2Clicked);
}

if (SubmitButton)
{
SubmitButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
SubmitButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnSubmitClicked);
}

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
if (HalfButton)
{
HalfButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnHalfClicked);
HalfButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnHalfClicked);
}
if (DieButton)
{
DieButton->OnClicked.RemoveDynamic(this, &USeotdaTempWidget::OnDieClicked);
DieButton->OnClicked.AddDynamic(this, &USeotdaTempWidget::OnDieClicked);
}
}

UTextBlock* USeotdaTempWidget::MakeText(const FName& WidgetName, const FString& InText, int32 FontSize)
{
UTextBlock* NewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
if (NewText)
{
NewText->SetText(FText::FromString(InText));
NewText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

FSlateFontInfo FontInfo = NewText->GetFont();
FontInfo.Size = FontSize;
NewText->SetFont(FontInfo);
}

return NewText;
}

UButton* USeotdaTempWidget::MakeButton(const FName& WidgetName, TObjectPtr<UTextBlock>& OutText, const FString& InText)
{
UButton* NewButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
OutText = MakeText(*FString::Printf(TEXT("%s_Text"), *WidgetName.ToString()), InText, 16);

if (NewButton && OutText)
{
OutText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
NewButton->SetContent(OutText.Get());
}

return NewButton;
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
if (HalfButton) HalfButton->SetIsEnabled(bEnabled);
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

    if (ResultText && PC->bSeotdaUiMatchEnded && !PC->SeotdaUiLastResultText.IsEmpty())
    {
        ResultText->SetText(FText::FromString(PC->SeotdaUiLastResultText));
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

UpdateCardButtonText(CardText0, 0, Cards);
UpdateCardButtonText(CardText1, 1, Cards);
UpdateCardButtonText(CardText2, 2, Cards);

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

void USeotdaTempWidget::UpdateCardButtonText(UTextBlock* TargetText, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards)
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
ToggleCardSelection(0);
}

void USeotdaTempWidget::OnCard1Clicked()
{
ToggleCardSelection(1);
}

void USeotdaTempWidget::OnCard2Clicked()
{
ToggleCardSelection(2);
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

void USeotdaTempWidget::OnHalfClicked()
{
RequestBetAction(EBettingAction::Half);
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
