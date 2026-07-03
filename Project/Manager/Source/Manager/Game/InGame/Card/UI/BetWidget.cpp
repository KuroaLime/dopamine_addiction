// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/UI/BetWidget.h"
#include "Manager.h"
#include "Components/Button.h"

void UBetWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Call)   Btn_Call->OnClicked.AddDynamic(this, &UBetWidget::OnCallBtnClick);
    if (Btn_Raise)  Btn_Raise->OnClicked.AddDynamic(this, &UBetWidget::OnRaiseBtnClick);
    if (Btn_Die)    Btn_Die->OnClicked.AddDynamic(this, &UBetWidget::OnDieBtnClick);
}

void UBetWidget::OnCallBtnClick() 
{
    DS_SCREEN(1, 1.1f, FColor::Yellow,
            TEXT("[Button] Call"));
}

void UBetWidget::OnRaiseBtnClick() 
{
    DS_SCREEN(1, 1.1f, FColor::Yellow,
            TEXT("[Button] Raise"));
}

void UBetWidget::OnDieBtnClick() 
{
    DS_SCREEN(1, 1.1f, FColor::Yellow,
            TEXT("[Button] Die"));
}