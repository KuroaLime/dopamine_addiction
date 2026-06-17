#include "Game/Lobby/UI/RoomWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Default/System/UManagerGameInstance.h"
#include "Game/Lobby/LobbyController.h"

void URoomWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LobbyButton)
    {
        LobbyButton->OnClicked.AddDynamic(this, &URoomWidget::OnLobbyButtonClicked);
    }

    if (ReadyButton)
    {
        ReadyButton->OnClicked.AddDynamic(this, &URoomWidget::OnReadyButtonClicked);
    }

    if (UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>())
    {
        GI->OnRoomMemberListUpdated.AddDynamic(this, &URoomWidget::UpdateRoomMemberState);
    }

    ApplyHostState(false);
}

void URoomWidget::NativeDestruct()
{
    if (UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>())
    {
        GI->OnRoomMemberListUpdated.RemoveDynamic(this, &URoomWidget::UpdateRoomMemberState);
    }

    Super::NativeDestruct();
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
    if (!PC)
    {
        return;
    }

    if (bIsHost)
    {
        PC->StartRoom();
    }
    else
    {
        PC->ToggleReadyState();
    }
}

void URoomWidget::UpdateRoomMemberState(const TArray<FRoomMemberInfoView>& Members)
{
    UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>();
    if (!GI)
    {
        ApplyHostState(false);
        return;
    }

    const uint32_t LocalSessionId = GI->GetSessionId();
    bool bLocalHost = false;

    for (const FRoomMemberInfoView& Member : Members)
    {
        if (static_cast<uint32_t>(Member.sessionId) == LocalSessionId)
        {
            bLocalHost = Member.isHost;
            break;
        }
    }

    ApplyHostState(bLocalHost);
}

void URoomWidget::ApplyHostState(bool bNewIsHost)
{
    bIsHost = bNewIsHost;
    SetReadyButtonLabel(bIsHost ? FText::FromString(TEXT("GameStart")) : FText::FromString(TEXT("Ready")));
}

void URoomWidget::SetReadyButtonLabel(const FText& NewText)
{
    if (!ReadyButton)
    {
        return;
    }

    if (UTextBlock* TextBlock = Cast<UTextBlock>(ReadyButton->GetContent()))
    {
        TextBlock->SetText(NewText);
    }
}
