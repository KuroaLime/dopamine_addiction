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

	// 서버가 이 클라이언트의 사격이 플레이어에게 명중했다고 확인해줬을 때 호출된다.
	void ShowHitMarker();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void StaticUI() override;
protected:
	void UpdateHPWidget();
	void UpdateNameWidget();
	void UpdateCardWidget();
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
	void UpdateCardSelectionHighlight();
	void HideHitMarker();

private:
	//HPbar
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HP_Bar = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HP_Text = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MaxHP_Text = nullptr;

	//NAME
	UPROPERTY(meta = (BindWidget))
	UTextBlock* NAMETxt = nullptr;

	//Card Image
	UPROPERTY(meta = (BindWidget))
	UImage* Card00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* Card01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* Card02 = nullptr;

	// Card00~02를 인덱스로 순회하기 위한 편의 배열(NativeConstruct에서 채움).
	UImage* CardImage[CardTotalNumber] = {};

	//Card Highlight (버릴 카드로 선택된 슬롯 뒤에 표시)
	UPROPERTY(meta = (BindWidget))
	UImage* CardHighlight00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* CardHighlight01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* CardHighlight02 = nullptr;

	UImage* CardHighlightImage[CardTotalNumber] = {};

	//Weapon Count
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WEAPONMAXTxt = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WEAPONCountxt = nullptr;

	//Aim
	UPROPERTY(meta = (BindWidget))
	UImage* Aim_Icon = nullptr;

	// 사격이 플레이어에게 명중했을 때 잠깐 표시되는 히트마커.
	UPROPERTY(meta = (BindWidget))
	UImage* HitMarker = nullptr;

	FTimerHandle HitMarkerTimerHandle;

	// 조준점 4방향 대시. Spread(블룸 진행도)에 따라 중심에서 바깥으로 이동시킨다.
	UPROPERTY(meta = (BindWidget))
	UImage* AimDash_Up = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* AimDash_Down = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* AimDash_Left = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* AimDash_Right = nullptr;

	// 현재 화면에 표시 중인 조준점 벌어짐 픽셀 값. 목표치(무기 블룸 각도 기반)를 향해 매 틱 부드럽게 보간된다.
	float CurrentDisplayedAimOffset = 0.f;

	// 디버그: Character/Weapon 조회 실패를 스팸 없이(0.5초에 한 번) 로그로 남기기 위한 누적 타이머.
	float AimDebugLogAccumulator = 0.f;

	//Level Icons
	UPROPERTY(meta = (BindWidget))
	UImage* LvHealth = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvHealthRegen = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvMoveSpeed = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvWeaponDamage = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvWeaponFireRate = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvWeaponRange = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvWeaponMagazine = nullptr;
	UPROPERTY(meta = (BindWidget))
	UImage* LvWeaponReload = nullptr;

	// LvHealth~LvWeaponReload를 인덱스로 순회하기 위한 편의 배열(NativeConstruct에서 채움).
	// 0: Health, 1: HealthRegen, 2: MoveSpeed, 3: WeaponDamage, 4: FireRate, 5: Range, 6: Magazine, 7: Reload
	UImage* Lv_Image[LvTotalNumber] = {};

	//Level Text (아이콘 위에 "LV. n" 형태로 표시)
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvHealthText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvHealthRegenText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvMoveSpeedText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvWeaponDamageText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvWeaponFireRateText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvWeaponRangeText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvWeaponMagazineText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LvWeaponReloadText = nullptr;

	// LvHealthText~LvWeaponReloadText를 인덱스로 순회하기 위한 편의 배열(NativeConstruct에서 채움). 순서는 Lv_Image와 동일.
	UTextBlock* LvText[LvTotalNumber] = {};

	//Compass
	UPROPERTY(meta = (BindWidget))
	UUserWidget* Compass = nullptr;

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
	class UMaterialInstanceDynamic* LvLinearMID[LvTotalNumber];

	static const FName LvLinearWipeParamName;
public:
	void CycleDiscardSelection();
	void ConfirmDiscardSelectedCard();
private:
	int32 DiscradSelectionIndex = -1;
};
