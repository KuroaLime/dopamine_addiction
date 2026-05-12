// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyController.h"
#include "Blueprint/UserWidget.h"

void ALobbyController::BeginPlay() {
	Super::BeginPlay();
	
	if (IsLocalController() && LobbyWidgetClass)
	{
		LobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
		if (LobbyWidget)
		{
			LobbyWidget->AddToViewport();

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
	}
}