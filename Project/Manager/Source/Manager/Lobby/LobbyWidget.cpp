// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "LobbyRoomWidget.h"
#include "LobbyController.h"

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
        if (TotalRoomNames.IsValidIndex(DataIndex))
        {
            RoomEntry->SetVisibility(ESlateVisibility::Visible);
            RoomEntry->UpdateRoomInfo(TotalRoomNames[DataIndex], 0, 8, nullptr);
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

void ULobbyWidget::OnRoomButtonClicked(FString SelectedRoomName)
{
	ALobbyController* PC = Cast<ALobbyController>(GetOwningPlayer());

    if (PC)
    {
        PC->JoinRoomSelected(SelectedRoomName);
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
        TotalRoomNames.Add(RoomName);
        MaxPage = (TotalRoomNames.Num() - 1) / 6;

        PC->CreateRoom(RoomName);
    }
}

void ULobbyWidget::RefreshRoomList(const TArray<FString>& NewRoomNames)
{
    TotalRoomNames = NewRoomNames;
    MaxPage = TotalRoomNames.Num() > 0 ? (TotalRoomNames.Num() - 1) / 6 : 0;

    if (CurrentPage > MaxPage)
    {
        CurrentPage = MaxPage;
    }

    UpdateRoomDisplay();
}