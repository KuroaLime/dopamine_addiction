// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Handler/UIHandler.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

UUIHandler::UUIHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UUIHandler::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController) return;

	CreateHUD();
}

void UUIHandler::UIActivate()
{
	ShowHUD();
}

void UUIHandler::UIDeactivate()
{
	HideHUD();
}

void UUIHandler::CreateHUD()
{
	if (!OwnerController) return;

	if (!OwnerController->IsLocalPlayerController()) return;

	if (!PlayerUI) return;

	PlayerWidget = CreateWidget<UUserWidget>(OwnerController, PlayerUI);
	if (PlayerWidget)
	{
		PlayerWidget->AddToViewport();
		PlayerWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUIHandler::ShowHUD()
{
	SetWidgetVisibility(ESlateVisibility::Visible);
}

void UUIHandler::HideHUD()
{
	SetWidgetVisibility(ESlateVisibility::Collapsed);
}

void UUIHandler::SetWidgetVisibility(ESlateVisibility Visibility)
{
	if (PlayerWidget)
		PlayerWidget->SetVisibility(Visibility);
}