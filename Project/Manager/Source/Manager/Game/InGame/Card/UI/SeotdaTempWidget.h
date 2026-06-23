#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Widgets/SWidget.h"
#include "SeotdaTempWidget.generated.h"

class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UButton;
class UTextBlock;

UCLASS()
class MANAGER_API USeotdaTempWidget : public UUserWidget
{
GENERATED_BODY()

protected:
virtual TSharedRef<SWidget> RebuildWidget() override;
virtual void NativeConstruct() override;
virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
UFUNCTION(BlueprintCallable, Category = "Seotda")
void RefreshFromPlayerState();

private:
UPROPERTY()
TObjectPtr<UBorder> RootBorder = nullptr;

UPROPERTY()
TObjectPtr<UVerticalBox> RootBox = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> TitleText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CardInfoText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> StatusText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> BetInfoText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> ResultText = nullptr;

UPROPERTY()
TObjectPtr<UButton> CardButton0 = nullptr;

UPROPERTY()
TObjectPtr<UButton> CardButton1 = nullptr;

UPROPERTY()
TObjectPtr<UButton> CardButton2 = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CardText0 = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CardText1 = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CardText2 = nullptr;

UPROPERTY()
TObjectPtr<UButton> SubmitButton = nullptr;

UPROPERTY()
TObjectPtr<UButton> CheckButton = nullptr;

UPROPERTY()
TObjectPtr<UButton> CallButton = nullptr;

UPROPERTY()
TObjectPtr<UButton> HalfButton = nullptr;

UPROPERTY()
TObjectPtr<UButton> DieButton = nullptr;

    UPROPERTY()
    TObjectPtr<UButton> LobbyButton = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> SubmitText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CheckText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> CallText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> HalfText = nullptr;

UPROPERTY()
TObjectPtr<UTextBlock> DieText = nullptr;

    UPROPERTY()
    TObjectPtr<UTextBlock> LobbyText = nullptr;

bool bSelected0 = false;
bool bSelected1 = false;
bool bSelected2 = false;

bool bLocalSelectionSubmitted = false;
double LastBetActionTimeSeconds = -1000.0;

TArray<int32> LastSeenCardInstanceIds;

private:
void BuildWidgetTree();
void BindButtonEvents();

UTextBlock* MakeText(const FName& WidgetName, const FString& InText, int32 FontSize);
UButton* MakeButton(const FName& WidgetName, TObjectPtr<UTextBlock>& OutText, const FString& InText);

void ResetLocalRoundUiState(const TArray<FOwnedCardInfo>& Cards);
void SetCardSelectionButtonsEnabled(bool bEnabled);
void SetBetButtonsEnabled(bool bEnabled);

void ToggleCardSelection(int32 CardIndex);
int32 GetSelectedCount() const;
void UpdateCardButtonText(UTextBlock* TargetText, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards);
void SubmitSelection();
void RequestBetAction(EBettingAction Action);

FString BuildCardIdListString(const TArray<int32>& Ids) const;

UFUNCTION()
void OnCard0Clicked();

UFUNCTION()
void OnCard1Clicked();

UFUNCTION()
void OnCard2Clicked();

UFUNCTION()
void OnSubmitClicked();

UFUNCTION()
void OnCheckClicked();

UFUNCTION()
void OnCallClicked();

UFUNCTION()
void OnHalfClicked();

UFUNCTION()
void OnDieClicked();

    UFUNCTION()
    void OnLobbyClicked();
};
