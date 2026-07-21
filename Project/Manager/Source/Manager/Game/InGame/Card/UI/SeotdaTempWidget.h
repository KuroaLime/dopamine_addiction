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
class UImage;
class UWidgetAnimation;
class UTexture2D;

UCLASS()
class MANAGER_API USeotdaTempWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Seotda")
	void RefreshFromPlayerState();

public:
	// ===== Root UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> RootBorder = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> RootBox = nullptr;

	// ===== Card Selection UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CardInfoText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CardButton0 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CardButton1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CardButton2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CardText0 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CardText1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CardText2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UImage> CardImage0 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UImage> CardImage1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UImage> CardImage2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SubmitButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SubmitText = nullptr;

	// ===== Status/Info UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BetInfoText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultText = nullptr;

	// ===== Betting UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CheckButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CheckText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CallButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CallText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> QuarterButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> QuarterText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> HalfButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> HalfText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> DdadangButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DdadangText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PpingButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PpingText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> AllInButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AllInText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> DieButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DieText = nullptr;

	// ===== Lobby UI =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UButton> LobbyButton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> LobbyText = nullptr;

	// ===== Opponent Seat UI (다른 플레이어 좌석 최대 4명) =====
	static constexpr int32 SeotdaOpponentSeatCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat0_NameText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat1_NameText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat2_NameText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat3_NameText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat0_ChipText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat1_ChipText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat2_ChipText = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Seat3_ChipText = nullptr;

	// 폴드(다이)한 좌석을 가리는 오버레이. 폴드 시 Visible, 아니면 Collapsed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat0_FoldedOverlay = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat1_FoldedOverlay = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat2_FoldedOverlay = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat3_FoldedOverlay = nullptr;

	// 현재 턴인 좌석을 강조하는 테두리/하이라이트. 해당 좌석 턴일 때만 Visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat0_TurnHighlight = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat1_TurnHighlight = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat2_TurnHighlight = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|UI|Seats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Seat3_TurnHighlight = nullptr;

	// 전체 좌석을 감싸는 루트(있으면 통째로 접어 숨기는 용도). 위 4묶음을 인덱스로 순회하기 위한 편의 배열.
	TObjectPtr<UTextBlock> SeatNameTexts[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UTextBlock> SeatChipTexts[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UWidget> SeatFoldedOverlays[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UWidget> SeatTurnHighlights[SeotdaOpponentSeatCount] = {};

	void RefreshOpponentSeats(class AMainPlayerController* PC);

	// ===== Runtime Data =====
	bool bSelected0 = false;
	bool bSelected1 = false;
	bool bSelected2 = false;

	bool bLocalRevealPending = false;
	bool bLocalSelectionPending = false;
	int32 LastHandledRevealResultSerial = 0;
	int32 LastHandledSelectionResultSerial = 0;
	bool bLastKnownRevealConfirmed = false;
	FString LocalSelectionFeedback;
	FString LastPublicCardVisualSignature;
	double LastBetActionTimeSeconds = -1000.0;

	TArray<int32> LastSeenCardInstanceIds;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PublicCardsTitleText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PublicCardsBox = nullptr;

	// ===== Card Image Map =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TMap<ECardID, UTexture2D*> CardImageMap;

	// ===== Card Animations =====
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card0_HoverAnim = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card0_SelectAnim = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card1_HoverAnim = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card1_SelectAnim = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card2_HoverAnim = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card2_SelectAnim = nullptr;

private:
	// ===== Widget Binding =====
	void BindWidgetsByName();

	template<typename WidgetType>
	void BindWidgetByName(TObjectPtr<WidgetType>& OutWidget, const FName& WidgetName)
	{
		OutWidget = Cast<WidgetType>(GetWidgetFromName(WidgetName));
	}

	// ===== Event Binding =====
	void BindButtonEvents();

	// ===== UI Logic =====
	void ResetLocalRoundUiState(const TArray<FOwnedCardInfo>& Cards);
	void SetCardSelectionButtonsEnabled(bool bEnabled);
	void SetBetButtonsEnabled(bool bEnabled);
	void ClearLocalCardSelection();
	void EnsurePublicCardsPanel();
	void RefreshPublicCardVisuals();

	void ToggleCardSelection(int32 CardIndex);
	int32 GetSelectedCount() const;
	void UpdateCardButtonText(UTextBlock* TargetText, UImage* CardImage, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards);
	void SubmitSelection();
	void RequestBetAction(EBettingAction Action);

	FString BuildCardIdListString(const TArray<int32>& Ids) const;
	FString BuildPublicCardSummary() const;

	// ===== Card Button Events =====
	UFUNCTION()
	void OnCard0Clicked();

	UFUNCTION()
	void OnCard0Hovered();

	UFUNCTION()
	void OnCard0Unhovered();

	UFUNCTION()
	void OnCard1Clicked();

	UFUNCTION()
	void OnCard1Hovered();

	UFUNCTION()
	void OnCard1Unhovered();

	UFUNCTION()
	void OnCard2Clicked();

	UFUNCTION()
	void OnCard2Hovered();

	UFUNCTION()
	void OnCard2Unhovered();

	// ===== Submit/Betting Events =====
	UFUNCTION()
	void OnSubmitClicked();

	UFUNCTION()
	void OnCheckClicked();

	UFUNCTION()
	void OnCallClicked();

	UFUNCTION()
	void OnQuarterClicked();

	UFUNCTION()
	void OnHalfClicked();

	UFUNCTION()
	void OnDdadangClicked();

	UFUNCTION()
	void OnPpingClicked();

	UFUNCTION()
	void OnAllInClicked();

	UFUNCTION()
	void OnDieClicked();

	// ===== Lobby Events =====
	UFUNCTION()
	void OnLobbyClicked();
};
