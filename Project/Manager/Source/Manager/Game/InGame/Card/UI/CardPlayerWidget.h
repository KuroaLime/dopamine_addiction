// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardPlayerWidget.generated.h"

class UButton;
class UTextBlock;
class UBetWidget;
class UCardButtonWidget;

UCLASS()
class MANAGER_API UCardPlayerWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UBetWidget* Btn_Bet;

	UPROPERTY(meta = (BindWidget))
	UCardButtonWidget* Btn_Card1;

	UPROPERTY(meta = (BindWidget))
	UCardButtonWidget* Btn_Card2;

	UPROPERTY(meta = (BindWidget))
	UCardButtonWidget* Btn_Card3;

	// 내 플레이어 정보 (좌측 하단)
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_MyName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_MyMoney;

private:

};
