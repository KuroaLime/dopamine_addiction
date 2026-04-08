// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WidgetParent.h"
#include "TpsCharacterWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UTpsCharacterWidget : public UWidgetParent
{
	GENERATED_BODY()
	
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

protected:
	void UpdateHPWidget();

private:
	UPROPERTY()
	class UProgressBar* HPProgressBar;
};
