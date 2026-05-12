// Fill out your copyright notice in the Description page of Project Settings.


#include "LoginPlayerController.h"
#include "Blueprint/UserWidget.h"

void ALoginPlayerController::BeginPlay() {
	Super::BeginPlay();
	
    if (IsLocalController() && LoginWidgetClass)
    {
        LoginWidget = CreateWidget<UUserWidget>(this, LoginWidgetClass);

        if (LoginWidget)
        {
            LoginWidget->AddToViewport();

            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(LoginWidget->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

            SetInputMode(InputMode);
            bShowMouseCursor = true;
        }
    }
}