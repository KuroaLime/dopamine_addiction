// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "TpsPlayerMainHUD.generated.h"

#define CardTotalNumber 3
#define LvTotalNumber 8
/**
 * 
 */
UCLASS()
class MANAGER_API UTpsPlayerMainHUD : public UWidgetParent
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void OnCardFlipMidpoint();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void StaticUI() override;
protected:
	void UpdateHPWidget();
	void UpdateNameWidget();
	void UpdateCardWidget();
	void UpdateWeaponIconWidget();
	void UpdateWeaponCountWidget();
	void ChangeCompassSize(float ZRotation, class UImage* PSU_Compass);
	void UpdateAim();
	void UpdateLevel();
	void UpdateLevel(const struct FPlayerData& PlayerData) { UpdateLevel(); }
	void UpdateLevel(const struct FAccumulatedUpgrades& Upgrades) { UpdateLevel(); }
	void UpdateCompass();

	void OnOwnedCardsChanged(const TArray<struct FOwnedCardInfo>& NewCards);
	void TryBindPlayerState();
	void TriggerCardFlip();
	
private:
	//HPbar
	UPROPERTY()
	UImage* HP_Image = nullptr;

	UPROPERTY()
	class UTextBlock* HPTxt = nullptr;
	UPROPERTY()
	UTextBlock* MaxHPTxt = nullptr;

	//NAME
	UPROPERTY()
	UTextBlock* NAMETxt = nullptr;

	//Card Image

	UPROPERTY()
	UImage* CardImage[CardTotalNumber];

	//Weapon Image
	UPROPERTY()
	UImage* WEAPONImage = nullptr;

	//Weapon Count
	UPROPERTY()
	UTextBlock* WEAPONMAXTxt = nullptr;
	UPROPERTY()
	UTextBlock* WEAPONCountxt = nullptr;

	//Aim
	UPROPERTY()
	UImage* Aim_Image = nullptr;

	UPROPERTY()
	UImage*	Lv_Image[LvTotalNumber];

	//Compass
	UPROPERTY()
	UUserWidget* TPS_Compass = nullptr;

	TWeakObjectPtr<class AMainPlayerState> CachedPlayerState;
	bool bNeedPlayerStateBind = false;

	ECardID Cards[CardTotalNumber];


	UPROPERTY(meta = (BindWidget))
	class UCRoundandTimerWidget* CRoundandTimer_UI;

	void UpdateCRoundandTimer_UI();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UTexture2D* PlayerIcon_Image = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* UsingWeapon_Images = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TMap<ECardID, UTexture2D*> CardTextureMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* EmptyCardTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TArray<UTexture2D*> Card_Images;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* CardFlipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* Aim_Images = nullptr;

	UPROPERTY()
	class UMaterialInstanceDynamic* HPCircleMID = nullptr;

	static const FName HPRadialWipeParamName;

	UPROPERTY()
	class UMaterialInstanceDynamic* LvLinearMID[LvTotalNumber];

	static const FName LvLinearWipeParamName;
public:
	void CycleDiscardSelection();
	void ConfirmDiscardSelectedCard();
private:
	int32 DiscradSelectionIndex = -1;
};
