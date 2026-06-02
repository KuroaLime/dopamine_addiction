// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/CardUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

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
	Super::UIActivate();
}

void UCardUIHandler::UIDeactivate()
{
	Super::UIDeactivate();
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