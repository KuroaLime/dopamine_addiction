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

	// ===== Runtime Data =====
	bool bSelected0 = false;
	bool bSelected1 = false;
	bool bSelected2 = false;

	bool bLocalSelectionSubmitted = false;
	double LastBetActionTimeSeconds = -1000.0;

	TArray<int32> LastSeenCardInstanceIds;

	// ===== Card Textures (공용 DataAsset) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda|Images", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCardTextureSet> CardTextures;

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
