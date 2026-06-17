// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/UI/RoomWidget.h"
#include "Components/Button.h"
#include "Game/Lobby/LobbyController.h"

void URoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (LobbyButton) LobbyButton->OnClicked.AddDynamic(this, &URoomWidget::OnLobbyButtonClicked);
	if (ReadyButton) ReadyButton->OnClicked.AddDynamic(this, &URoomWidget::OnReadyButtonClicked);
}

void URoomWidget::OnLobbyButtonClicked()
{
	ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());
	if (PC)
	{
		PC->LeaveRoom();
	}
}

void URoomWidget::OnReadyButtonClicked()
{
	ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());
	if (PC)
	{
		PC->ToggleReadyState();
	}
}