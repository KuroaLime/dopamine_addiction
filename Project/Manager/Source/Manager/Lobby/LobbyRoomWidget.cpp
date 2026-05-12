// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/LobbyRoomWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void ULobbyRoomWidget::UpdateRoomInfo(const FString& Name, int32 CurrentPlayers, int32 MaxPlayers, UTexture2D* Image)
{
	if (RoomNameText) RoomNameText->SetText(FText::FromString(Name));
	if (PlayerCountText) PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPlayers, MaxPlayers)));
	if (RoomImage) RoomImage->SetBrushFromTexture(Image);
}