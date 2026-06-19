// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/UI/CardPlayerWidget.h"
#include "Game/InGame/Card/UI/BetWidget.h"
#include "Game/InGame/Card/UI/CardButtonWidget.h"
#include "Components/TextBlock.h"

void UCardPlayerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Txt_MyName)  Txt_MyName->SetText(FText::FromString(TEXT("PLAYER")));
    if (Txt_MyMoney) Txt_MyMoney->SetText(FText::FromString(TEXT("$ 100,000,000")));
}