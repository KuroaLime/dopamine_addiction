// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "ShopButton.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPurchaseButtonClicked,int32,ItemID );

UCLASS()
class MANAGER_API UShopButton : public UWidgetParent
{
	GENERATED_BODY()
private:
	UPROPERTY(meta = (BindWidget))
	class UImage* Item_Image = nullptr;

	UPROPERTY(meta = (BindWidget))
	class  UTextBlock* Item_Name = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Item_Price = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UButton* Purchase_Button = nullptr;

	int32 ButtonItemID = -1;

	UFUNCTION()
	void OnPurchaseButtonClicked();
public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPurchaseButtonClicked OnPurchaseEvent;

	void SetItemID(int32 NewID);

	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

public:
	void UpdateWidget();
};
