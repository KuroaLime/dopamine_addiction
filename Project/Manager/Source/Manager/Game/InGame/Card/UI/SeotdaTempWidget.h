#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Widgets/SWidget.h"
#include "SeotdaTempWidget.generated.h"

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
	// ===== Card Selection UI =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> InfoTxt = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Card0Btn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Card1Btn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Card2Btn = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Card0Txt = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Card1Txt = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Card2Txt = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Card0Img = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Card1Img = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Card2Img = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SubmitBtn = nullptr;

	// ===== Top Bar (라운드 · 팟 · 현재턴) =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoundTxt = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PotTxt = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TurnTxt = nullptr;

	// ===== Betting UI =====
	// 각 버튼의 캡션("체크"/"콜" 등)은 항상 고정 텍스트라 C++이 갱신할 일이 없다.
	// 버튼 라벨은 바인딩 없이 버튼 내부에 텍스트를 직접 넣으면 된다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CheckBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CallBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuarterBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HalfBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DdadangBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PpingBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AllInBtn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DieBtn = nullptr;

	// ===== Lobby UI =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LobbyBtn = nullptr;

	// ===== Opponent Seat UI (다른 플레이어 좌석 최대 4명) =====
	static constexpr int32 SeotdaOpponentSeatCount = 4;

	// 좌석 배경(두루마리 장식). 그 자리에 상대가 없으면 이것도 같이 숨긴다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat0Bg = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat1Bg = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat2Bg = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat3Bg = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat0Name = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat1Name = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat2Name = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat3Name = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat0Chip = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat1Chip = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat2Chip = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Seat3Chip = nullptr;

	// 현재 턴인 좌석을 강조하는 테두리/하이라이트. 해당 좌석 턴일 때만 Visible.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat0Turn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat1Turn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat2Turn = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Seat3Turn = nullptr;

	// 그 좌석 플레이어가 현재 공개한 카드(있으면). 없으면 Collapsed.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Seat0Card = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Seat1Card = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Seat2Card = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Seat3Card = nullptr;

	// Seat0~3을 인덱스로 순회하기 위한 편의 배열(NativeConstruct에서 채움).
	TObjectPtr<UWidget> SeatBackgrounds[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UTextBlock> SeatNameTexts[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UTextBlock> SeatChipTexts[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UWidget> SeatTurnHighlights[SeotdaOpponentSeatCount] = {};
	TObjectPtr<UImage> SeatCardImages[SeotdaOpponentSeatCount] = {};

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
	double LastBetActionTimeSeconds = -1000.0;

	TArray<int32> LastSeenCardInstanceIds;

	// ===== Card Textures (공용 DataAsset) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCardTextureSet> CardTextures;

	// ===== Button Skin (일반/호버/클릭 상태별 텍스처, 모든 버튼에 공용 적용) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ButtonNormalTexture = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ButtonHoveredTexture = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ButtonPressedTexture = nullptr;

	// ===== Card Animations =====
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card0Hover = nullptr;
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card0Select = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card1Hover = nullptr;
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card1Select = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card2Hover = nullptr;
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	TObjectPtr<UWidgetAnimation> Card2Select = nullptr;

private:
	// ===== Event Binding =====
	void BindButtonEvents();
	void ApplyButtonSkin();

	// ===== UI Logic =====
	void ResetLocalRoundUiState(const TArray<FOwnedCardInfo>& Cards);
	void SetCardSelectionButtonsEnabled(bool bEnabled);
	void SetBetButtonsEnabled(bool bEnabled);
	void ClearLocalCardSelection();

	void ToggleCardSelection(int32 CardIndex);
	int32 GetSelectedCount() const;
	void UpdateCardButtonText(UTextBlock* TargetText, UImage* CardImage, int32 CardIndex, const TArray<FOwnedCardInfo>& Cards);
	void SubmitSelection();
	void RequestBetAction(EBettingAction Action);

	FString BuildCardIdListString(const TArray<int32>& Ids) const;

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
