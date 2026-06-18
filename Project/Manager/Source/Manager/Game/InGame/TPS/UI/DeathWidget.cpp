// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/DeathWidget.h"
#include "Components/TextBlock.h"

void UDeathWidget::UpdateTime(int32 time) const
{
    if (!ResponeTimeText) return;

    FString TimeStr = FString::Printf(TEXT("%d"), time);
    ResponeTimeText->SetText(FText::FromString(TimeStr));
}

void UDeathWidget::UpdateResponeTime(const int& time)
{
    if (!ResponeTimeText) return;

    FString TimeStr = FString::Printf(TEXT("%d"), time);
    ResponeTimeText->SetText(FText::FromString(TimeStr));
}