// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyController.h"
#include "Blueprint/UserWidget.h"

void ALobbyController::BeginPlay() {
	Super::BeginPlay();

	if (IsLocalController())
	{
		for (auto& Pair : LobbyWidgetClass) {
			ELobbyState State = Pair.Key;
			TSubclassOf<UUserWidget> Class = Pair.Value;

			if (Class) {
				UUserWidget* NewWidget = CreateWidget<UUserWidget>(this, Class);
				if (NewWidget) {

					NewWidget->AddToViewport();
					NewWidget->SetVisibility(ESlateVisibility::Collapsed);

					WidgetInstances.Add(State, NewWidget);
				}
			}
		}
		ToggleLobbyUI(true, ELobbyState::RoomList);
	}


}

void ALobbyController::ToggleLobbyUI(bool bSucceed, ELobbyState NewState) {
	if(bSucceed)
	{
		if (CurrentWidget) {
			CurrentWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		//UUserWidget** FoundWidgetPtr = WidgetInstances.Find(NewState);


		if (WidgetInstances.Contains(NewState)) {
			WidgetInstances[NewState]->SetVisibility(ESlateVisibility::Visible);
			CurrentWidget = WidgetInstances[NewState];

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(CurrentWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
	}

}