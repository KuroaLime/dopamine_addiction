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

	// 발사 즉시(Tick을 기다리지 않고) 조준점 블룸 표시를 갱신하기 위한 공개 진입점.
	void RefreshAimSpread() { UpdateAim(0.f); }

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
	void UpdateAim(float DeltaTime);
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

	// 조준점 4방향 대시. Spread(블룸 진행도)에 따라 중심에서 바깥으로 이동시킨다.
	UPROPERTY()
	UImage* Aim_Up = nullptr;
	UPROPERTY()
	UImage* Aim_Down = nullptr;
	UPROPERTY()
	UImage* Aim_Left = nullptr;
	UPROPERTY()
	UImage* Aim_Right = nullptr;

	// 현재 화면에 표시 중인 조준점 벌어짐 픽셀 값. 목표치(무기 블룸 각도 기반)를 향해 매 틱 부드럽게 보간된다.
	float CurrentDisplayedAimOffset = 0.f;

	// 디버그: Character/Weapon 조회 실패를 스팸 없이(0.5초에 한 번) 로그로 남기기 위한 누적 타이머.
	float AimDebugLogAccumulator = 0.f;

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
