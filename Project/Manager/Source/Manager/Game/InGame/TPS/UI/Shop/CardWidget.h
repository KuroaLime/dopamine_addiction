// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
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

	int32 CardID = -1;

	UFUNCTION()
	void OnSelectCardClicked();
	UFUNCTION()
	void OnSelectCardHover();
public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCardSelectionButtonClicked OnCardSelectionEvent;

	void SetCardID(int32 NewID);

	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

public:
	void UpdateWidget();
};
