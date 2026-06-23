// Fill out your copyright notice in the Description page of Project Settings.


#include "Game\Lobby\UI\LobbyRoomWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Game\Lobby\LobbyController.h"

void ULobbyRoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (EntryButton)
	{
		EntryButton->OnClicked.AddDynamic(this, &ULobbyRoomWidget::OnEntryButtonClicked);
	}
}

void ULobbyRoomWidget::UpdateRoomInfo(const RoomInfoView& RoomData, UTexture2D* Image)
{
	RoomInfo = RoomData;
	if (RoomNameText) RoomNameText->SetText(FText::FromString(RoomData.title));
	if (PlayerCountText) PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), RoomData.curPlayers, RoomData.maxPlayers)));
	if (RoomImage) RoomImage->SetBrushFromTexture(Image);
}

void ULobbyRoomWidget::OnEntryButtonClicked()
{
	ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());
	if (PC)
	{
		PC->JoinRoomSelected(RoomInfo.roomId);
	}
}