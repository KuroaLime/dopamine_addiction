// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Login/LoginPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Default/System/UManagerGameInstance.h"


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

void ALoginPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void ALoginPlayerController::OnUnPossess()
{
    Super::OnUnPossess();
}

///////////////////////////////////////////////////////////////////////////////
// Client Sends : [PacketType]
///////////////////////////////////////////////////////////////////////////////


void ALoginPlayerController::ClientLogin(const FString& id, const FString& pw)
{
    GetGameInstance<UUManagerGameInstance>()->SendLogin(id, pw);
}