// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameStateBase.h"
#include "Default/System/UManagerGameInstance.h"
#include "Game/Lobby/UI/LobbyWidget.h"
//#include "Game/Lobby/GS_Lobby.h"

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
		        UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>();
        if (GI && GI->ConsumeReturnToRoomAfterMatch())
        {
            UE_LOG(LogTemp, Warning, TEXT("[LOBBY_RETURN] LobbyController BeginPlay -> InRoom"));
            ToggleLobbyUI(true, ELobbyState::InRoom);
            MoveLobbyCamera(RoomLocation);
            GI->BroadcastCachedRoomMembers();
        }
        else
        {
            ToggleLobbyUI(true, ELobbyState::RoomList);
        }

        UpdateRoom();
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

void ALobbyController::TransitionToLobbyState(bool bSucceed, ELobbyState LobbyState)
{
	switch (LobbyState)
	{
	case ELobbyState::InRoom:
	{
		ToggleLobbyUI(bSucceed, ELobbyState::InRoom);
		MoveLobbyCamera(RoomLocation);
		break;
	}

	case ELobbyState::RoomList:
	{
		ToggleLobbyUI(bSucceed, ELobbyState::RoomList);
		MoveLobbyCamera(LobbyLocation);
		break;
	}

	case ELobbyState::Settings:
	{
		break;
	}

	default:
	{
		break;
	}
	}
}

void ALobbyController::JoinRoomSelected(uint32_t RoomId)
{
	GetGameInstance<UUManagerGameInstance>()->SendJoinRoom(RoomId);
}

void ALobbyController::LeaveRoom()
{
	bReady = false;
	GetGameInstance<UUManagerGameInstance>()->SendLeaveRoom();
}

void ALobbyController::StartRoom()
{
	GetGameInstance<UUManagerGameInstance>()->SendRoomStart();
}

void ALobbyController::CreateRoom(const FString& RoomName)
{
	GetGameInstance<UUManagerGameInstance>()->SendCreateRoom(RoomName);
}

void ALobbyController::UpdateRoom()
{
	GetGameInstance<UUManagerGameInstance>()->SendRoomListReq();
}

void ALobbyController::Client_GetRoomList(TArray<RoomInfoView> RoomList)
{
	if (UUserWidget* ListWidget = WidgetInstances.FindRef(ELobbyState::RoomList))
	{
		if(ULobbyWidget* LobbyWGT = Cast<ULobbyWidget>(ListWidget))
		{
			LobbyWGT->UpdateRoomList(RoomList);
		}
	}
}

void ALobbyController::ToggleReadyState()
{
	bReady = !bReady;
	GetGameInstance<UUManagerGameInstance>()->SendReady(bReady);
}