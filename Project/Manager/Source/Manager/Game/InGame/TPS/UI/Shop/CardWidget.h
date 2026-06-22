// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

#include "CardWidget.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardSelectionButtonClicked, int32, ItemID);

UCLASS()
class MANAGER_API UCardWidget : public UWidgetParent
{
	GENERATED_BODY()
private:

	UPROPERTY(meta = (BindWidget))
	class UImage* Selection_Backgorund = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UImage* Selection_Icon = nullptr;


	UPROPERTY(meta = (BindWidget))
	class  UTextBlock* Card_Name = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Card_Descriptor = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UButton* Selection_Button = nullptr;

	FRandomCardOption CurrentOption;

	int32 SelectionIndex = -1;

	UFUNCTION()
	void OnSelectCardClicked();
	UFUNCTION()
	void OnSelectCardHover();
public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCardSelectionButtonClicked OnCardSelectionEvent;

	void SetUpgradeType(const FRandomCardOption& NewOption, int32 Index);

	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

public:
	void UpdateWidget();

protected:
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* HoverAnim;
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* FlipAnim;

	virtual void NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	bool bIsFaceUp = true;
public:
	bool GetbIsFaceUp() { return bIsFaceUp; };
};
