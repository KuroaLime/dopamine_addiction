// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
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

    // 임시 데이터 15개 생성 (테스트용)
    for (int32 i = 1; i <= 15; ++i) TotalRoomNames.Add(FString::Printf(TEXT("Room %d"), i));

    MaxPage = (TotalRoomNames.Num() - 1) / 6;

    NextPageButton->OnClicked.AddDynamic(this, &ULobbyWidget::OnNextPageClicked);
    PrevPageButton->OnClicked.AddDynamic(this, &ULobbyWidget::OnPrevPageClicked);
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