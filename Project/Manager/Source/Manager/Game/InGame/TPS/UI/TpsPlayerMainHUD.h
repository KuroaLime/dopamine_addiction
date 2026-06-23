// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "TpsPlayerMainHUD.generated.h"

#define CardTotalNumber 3
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
	void UpdateCompass();

	void OnOwnedCardsChanged(const TArray<struct FOwnedCardInfo>& NewCards);
	void TryBindPlayerState();
	void TriggerCardFlip();
	
private:
	//HP progress bar
	UPROPERTY()
	class UProgressBar* HPProgressBar = nullptr;

	//HP TXT
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
	UTextBlock* WEAPONMaxTxt = nullptr;
	UPROPERTY()
	UTextBlock* WEAPONCountxt = nullptr;

	//Compass
	UPROPERTY()
	UUserWidget* TPS_Compass = nullptr;

	TWeakObjectPtr<class AMainPlayerState> CachedPlayerState;
	bool bNeedPlayerStateBind = false;

	ECardID Cards[CardTotalNumber];
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UTexture2D* PlayerIcon_Image = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* UsingWeapon_Images = nullptr;

	// ECardID ������� ���ε� �ؽ�ó (��������Ʈ���� ����)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TMap<ECardID, UTexture2D*> CardTextureMap;

	// ������ ����� �� ǥ���� �⺻ �ؽ�ó
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* EmptyCardTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TArray<UTexture2D*> Card_Images;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* CardFlipAnim;
};
