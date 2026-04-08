// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetParent.h"
#include "WG_HavingCoin.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UWG_HavingCoin : public UWidgetParent
{
	GENERATED_BODY()

public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
protected:
	void UpdateHavingCoinWidget();

private:
	UPROPERTY()
	class UTextBlock* HavingCoinText;
};
