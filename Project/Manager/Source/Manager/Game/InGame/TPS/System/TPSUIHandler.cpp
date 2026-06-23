// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Interface/UIInterface.h"
#include "Blueprint/UserWidget.h"

UTPSUIHandler::UTPSUIHandler()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTPSUIHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UTPSUIHandler::UIActivate()
{
	Super::UIActivate();
}

void UTPSUIHandler::UIDeactivate()
{
	Super::UIDeactivate();
}

void UTPSUIHandler::SetUITimer(int32 time)
{
	Super::SetUITimer(time);

	IUIInterface* UII =
		Cast<IUIInterface>(PlayerWidget);
	if (!UII) return;

	UII->UpdateTime(time);
}

void UTPSUIHandler::CreateHUD()
{
	Super::CreateHUD();
}

void UTPSUIHandler::ShowHUD()
{
	Super::ShowHUD();
}

void UTPSUIHandler::HideHUD()
{
	Super::HideHUD();
}

void UTPSUIHandler::UIToggle()
{
	Super::UIToggle();
}