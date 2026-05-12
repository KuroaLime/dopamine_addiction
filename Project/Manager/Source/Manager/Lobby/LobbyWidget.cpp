// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "LobbyRoomWidget.h"

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
        if (!RoomButtons.IsValidIndex(i) || !RoomButtons[i]) continue;

        int32 DataIndex = StartIndex + i;

        if (TotalRoomNames.IsValidIndex(DataIndex))
        {
            RoomButtons[i]->SetVisibility(ESlateVisibility::Visible);

            // 여기서 방 데이터를 전달합니다. 
            // (실제 데이터 구조체가 있다면 그걸 넘겨주는 게 더 좋습니다)
            RoomButtons[i]->UpdateRoomInfo(TotalRoomNames[DataIndex], 3, 4, nullptr);
        }
        else
        {
            RoomButtons[i]->SetVisibility(ESlateVisibility::Hidden);
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