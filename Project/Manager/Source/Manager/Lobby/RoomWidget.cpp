// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/RoomWidget.h"
#include "Components/Button.h"
#include "LobbyController.h"

void URoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (LobbyButton) LobbyButton->OnClicked.AddDynamic(this, &URoomWidget::OnLobbyButtonClicked);
}

void URoomWidget::OnLobbyButtonClicked()
{
	ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());
	if (PC)
	{
		PC->LeaveRoom();
	}
}