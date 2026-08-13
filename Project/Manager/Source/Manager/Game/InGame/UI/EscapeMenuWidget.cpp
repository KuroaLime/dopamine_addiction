// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/UI/EscapeMenuWidget.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Game/InGame/MainPlayerController.h"

void UEscapeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (GraphicsTab)
	{
		GraphicsTab->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnGraphicsTabClicked);
	}
	if (SoundTab)
	{
		SoundTab->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnSoundTabClicked);
	}
	if (ThirdTab)
	{
		ThirdTab->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnThirdTabClicked);
	}

	if (ExitToLobbyButton)
	{
		ExitToLobbyButton->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnExitToLobbyClicked);
	}
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UEscapeMenuWidget::OnResumeClicked);
	}

	// 기본으로 첫 번째 콘텐츠(그래픽)를 보여준다.
	ShowContentIndex(0);
}

void UEscapeMenuWidget::ShowContentIndex(int32 Index)
{
	if (ContentSwitcher && Index >= 0 && Index < ContentSwitcher->GetNumWidgets())
	{
		ContentSwitcher->SetActiveWidgetIndex(Index);
	}
}

void UEscapeMenuWidget::OnGraphicsTabClicked()
{
	ShowContentIndex(0);
}

void UEscapeMenuWidget::OnSoundTabClicked()
{
	ShowContentIndex(1);
}

void UEscapeMenuWidget::OnThirdTabClicked()
{
	ShowContentIndex(2);
}

void UEscapeMenuWidget::OnExitToLobbyClicked()
{
	if (AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		// 로비 복귀 경로는 매치 종료 시와 동일하다(GI 복귀표시 → 위젯정리 → Lobby_Stage 로드).
		PC->ReturnToLobbyFromMatchEnd();
	}
}

void UEscapeMenuWidget::OnResumeClicked()
{
	if (AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		PC->CloseEscapeMenu();
	}
}
