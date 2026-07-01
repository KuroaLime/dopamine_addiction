// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/UI/CardButtonWidget.h"
#include "Components/Button.h"

void UCardButtonWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Card)
    {
        Btn_Card->OnClicked.AddDynamic(this, &UCardButtonWidget::OnCardBtnClicked);
    }
}

void UCardButtonWidget::OnCardBtnClicked()
{
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(1, 1.1f, FColor::Yellow,
            TEXT("[Button] Card Select"));
}