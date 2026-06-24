// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/Card/CardUIHandler.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h" 
#include "Game/InGame/Card/UI/SeotdaTempWidget.h"
#include "GameFramework/PlayerController.h"

UCardUIHandler::UCardUIHandler()
{
PrimaryComponentTick.bCanEverTick = true;
}

void UCardUIHandler::BeginPlay()
{

Super::BeginPlay();
}

void UCardUIHandler::UIActivate()
{
ShowHUD();
}

void UCardUIHandler::UIDeactivate()
{

HideHUD();
}

void UCardUIHandler::SetUITimer(int32 time)
{
Super::SetUITimer(time);
}

void UCardUIHandler::CreateHUD()
{
Super::CreateHUD();
}

void UCardUIHandler::ShowHUD()
{
Super::ShowHUD();
}

void UCardUIHandler::HideHUD()
{
Super::HideHUD();

}
