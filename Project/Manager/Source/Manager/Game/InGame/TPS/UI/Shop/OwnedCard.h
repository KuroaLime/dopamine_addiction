// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "OwnedCard.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UOwnedCard : public UWidgetParent
{
	GENERATED_BODY()
private:

	UPROPERTY(meta = (BindWidget))
	class UImage* Selection_Backgorund = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UButton* Selection_Button = nullptr;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card | Setup")
	class UTexture2D* CardBackTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card | Setup")
	class UTexture2D* CardFrontTexture = nullptr;


protected:
	virtual void NativeConstruct() override;


protected:
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* HoverAnim;
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* ClickAnim;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* CardFlipAnim;

	virtual void NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;
	UFUNCTION()
	void OnCardButtonClicked();
public:
	void SetUpgradeType(int32 CardValue, int32 Index);
public:
	void SetCardData(const FOwnedCardInfo& CardInfo, class UTexture2D* CardTexture);
	void ClearCard();
protected:
	UFUNCTION(BlueprintCallable, Category = "Card")
	void OnCardFlipMidpoint();
	UFUNCTION()
	void PlayFlipAnimation();
private:
	FOwnedCardInfo CachedCardInfo;

	bool bIsFaceUp = true;
	FTimerHandle ClickSequenceTimerHandle;
	FTimerHandle FlipTimerHandle;
};
