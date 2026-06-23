// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "GroupOwnedCard.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UGroupOwnedCard : public UWidgetParent
{
	GENERATED_BODY()
private:
	UPROPERTY(meta = (BindWidget))
	class UImage* Background = nullptr;


	UPROPERTY(meta = (BindWidget))
	class  UTextBlock* Title = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Info = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard02 = nullptr;
};
