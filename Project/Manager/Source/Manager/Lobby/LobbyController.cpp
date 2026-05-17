// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameStateBase.h"
#include "Game/Lobby/GS_Lobby.h"

void ALobbyController::BeginPlay() {
	Super::BeginPlay();
	
	if (IsLocalController())
	{
		if (APawn* LobbyPawn = GetPawn())
		{
			LobbyLocation = LobbyPawn->GetActorLocation();
		}

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

void ALobbyController::ProcessInterpolatedMovement()
{
	APawn* LobbyPawn = GetPawn();
	if (!LobbyPawn)
	{
		GetWorldTimerManager().ClearTimer(MovementTimerHandle);
		return;
	}

	InterpAlpha += (0.01f / TravelDuration);

	float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, InterpAlpha, 2.0f);
	FVector NewLocation = FMath::Lerp(StartLocation, TargetLocation, SmoothAlpha);

	LobbyPawn->SetActorLocation(NewLocation);

	if (InterpAlpha >= 1.0f)
	{
		LobbyPawn->SetActorLocation(TargetLocation);
		GetWorldTimerManager().ClearTimer(MovementTimerHandle);
	}
}

void ALobbyController::SwitchToRoomUI(bool bIsInsideRoom)
{
	if (bIsInsideRoom)
	{
		ToggleLobbyUI(true, ELobbyState::InRoom);
	}
	else
	{
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

void ALobbyController::MoveLobbyCamera(FVector NewTarget)
{
	APawn* LobbyPawn = GetPawn();
	if (!LobbyPawn) return;

	StartLocation = LobbyPawn->GetActorLocation();
	TargetLocation = NewTarget;
	InterpAlpha = 0.0f;

	GetWorldTimerManager().ClearTimer(MovementTimerHandle);

	GetWorldTimerManager().SetTimer(MovementTimerHandle, this, &ALobbyController::ProcessInterpolatedMovement, 0.01f, true);
}

void ALobbyController::JoinRoomSelected(FString RoomName)
{
	ToggleLobbyUI(true, ELobbyState::InRoom);

	MoveLobbyCamera(RoomLocation);
}

void ALobbyController::LeaveRoom()
{
	ToggleLobbyUI(true, ELobbyState::RoomList);

	MoveLobbyCamera(LobbyLocation);
}

void ALobbyController::CreateRoom(const FString& RoomName)
{
	Server_CreateRoom(RoomName);

	ToggleLobbyUI(true, ELobbyState::InRoom);
	MoveLobbyCamera(RoomLocation);
}

void ALobbyController::Server_CreateRoom_Implementation(const FString& RoomName) {
	if (AGS_Lobby* GS = Cast<AGS_Lobby>(GetWorld()->GetGameState())) {
		GS->AddRoomName(RoomName);
	}
}

bool ALobbyController::Server_CreateRoom_Validate(const FString& RoomName) {
	return true;
}