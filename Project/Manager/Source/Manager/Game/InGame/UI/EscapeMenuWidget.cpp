// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/UI/EscapeMenuWidget.h"
#include "Components/Button.h"
#include "Game/InGame/MainPlayerController.h"

void UEscapeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ExitToLobbyButton)
	{
		ExitToLobbyButton->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnExitToLobbyClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnSettingsClicked);
	}
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnResumeClicked);
	}
}

void UEscapeMenuWidget::OnExitToLobbyClicked()
{
	if (AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		// 로비 복귀 경로는 매치 종료 시와 동일하다(GI 복귀표시 → 위젯정리 → Lobby_Stage 로드).
		PC->ReturnToLobbyFromMatchEnd();
	}
}

void UEscapeMenuWidget::OnSettingsClicked()
{
	// TODO: 설정 화면. 요청에 따라 이번엔 동작을 구현하지 않는다(자리만 마련).
}

void UEscapeMenuWidget::OnResumeClicked()
{
	if (AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		PC->CloseEscapeMenu();
	}
}
