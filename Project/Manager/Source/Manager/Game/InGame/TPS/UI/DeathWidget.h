// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/InGame/Interface/UIInterface.h"
#include "DeathWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UDeathWidget : public UUserWidget,
								 public IUIInterface
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ResponeTimeText;

public:
	void UpdateTime(int32 time) const override;

public:
	void UpdateResponeTime(const int& time);
};
