// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/UI/LobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Game/Lobby/UI/LobbyRoomWidget.h"
#include "Game/Lobby/LobbyController.h"
#include "Default/System/UManagerGameInstance.h"

void ULobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RoomButtons.Empty();
    RoomButtons.Add(RoomButton_0);
    RoomButtons.Add(RoomButton_1);
    RoomButtons.Add(RoomButton_2);
    RoomButtons.Add(RoomButton_3);
    RoomButtons.Add(RoomButton_4);
    RoomButtons.Add(RoomButton_5);

    NextPageButton->OnClicked.AddDynamic(this, &ULobbyWidget::OnNextPageClicked);
    PrevPageButton->OnClicked.AddDynamic(this, &ULobbyWidget::OnPrevPageClicked);
	CreateRoomButton->OnClicked.AddDynamic(this, &ULobbyWidget::OnCreateRoomClicked);
	UpdateRoomButton->OnClicked.AddDynamic(this, &ULobbyWidget::UpdateRoomDisplay);

    UpdateRoomDisplay();
}

void ULobbyWidget::UpdateRoomDisplay()
{
    int32 StartIndex = CurrentPage * 6;

    for (int32 i = 0; i < 6; ++i)
    {
        ULobbyRoomWidget* RoomEntry = Cast<ULobbyRoomWidget>(RoomButtons[i]);
        if (!RoomEntry) continue;

        int32 DataIndex = (CurrentPage * 6) + i;
        if (RoomList.IsValidIndex(DataIndex))
        {
            RoomEntry->SetVisibility(ESlateVisibility::Visible);
            RoomEntry->UpdateRoomInfo(RoomList[DataIndex], nullptr);
        }
        else
        {
            RoomEntry->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    PageText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPage + 1, MaxPage + 1)));

    if (PrevPageButton) PrevPageButton->SetIsEnabled(CurrentPage > 0);
    if (NextPageButton) NextPageButton->SetIsEnabled(CurrentPage < MaxPage);
}

void ULobbyWidget::OnNextPageClicked()
{
    if (CurrentPage < MaxPage)
    {
        CurrentPage++;
        UpdateRoomDisplay();
    }
}

void ULobbyWidget::OnPrevPageClicked()
{
    if (CurrentPage > 0)
    {
        CurrentPage--;
        UpdateRoomDisplay();
    }
}

void ULobbyWidget::OnCreateRoomClicked()
{
    FString RoomName = TEXT("New Room");
    if (RoomNameInputText)
    {
        RoomName = RoomNameInputText->GetText().ToString();
        if (RoomName.IsEmpty()) return;
    }

    ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());
    if (PC)
    {
        PC->CreateRoom(RoomName);
		UpdateRoomDisplay();
    }
}

void ULobbyWidget::UpdateRoomList(TArray<RoomInfoView>& TArr_RoomList)
{
    RoomList = TArr_RoomList;
    MaxPage = RoomList.Num() > 0 ? (RoomList.Num() - 1) / 6 : 0;

    if (CurrentPage > MaxPage)
    {
        CurrentPage = MaxPage;
    }

    UpdateRoomDisplay();
}