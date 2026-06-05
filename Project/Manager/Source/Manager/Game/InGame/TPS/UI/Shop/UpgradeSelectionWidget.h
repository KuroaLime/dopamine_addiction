// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "UpgradeSelectionWidget.generated.h"

/**
 * 
 */


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionFinished);

UCLASS()
class MANAGER_API UUpgradeSelectionWidget : public UWidgetParent
{
	GENERATED_BODY()
private:
	
	UPROPERTY(meta = (BindWidget))
	class UCardWidget* SelectionCard00 = nullptr;
	
	UPROPERTY(meta = (BindWidget))
	class UCardWidget* SelectionCard01 = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UCardWidget* SelectionCard02 = nullptr;



	UFUNCTION()
	void HandleCardSelected(int32 CardID);



public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSelectionFinished OnSelectionFinishedEvent;

	void SetCardID(int32 NewID);

	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

public:
	void UpdateWidget();
};
