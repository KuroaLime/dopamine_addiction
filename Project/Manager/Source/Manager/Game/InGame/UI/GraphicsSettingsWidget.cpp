// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/UI/GraphicsSettingsWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"

void UGraphicsSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitScreenModes();
	InitResolutions();

	if (ScreenModeLeftButton)  { ScreenModeLeftButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnScreenModePrev); }
	if (ScreenModeRightButton) { ScreenModeRightButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnScreenModeNext); }
	if (ResolutionLeftButton)  { ResolutionLeftButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnResolutionPrev); }
	if (ResolutionRightButton) { ResolutionRightButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnResolutionNext); }

	RefreshScreenModeText();
	RefreshResolutionText();
}

UGameUserSettings* UGraphicsSettingsWidget::GetSettings() const
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

void UGraphicsSettingsWidget::InitScreenModes()
{
	ScreenModes.Reset();
	ScreenModeLabels.Reset();

	ScreenModes.Add(EWindowMode::Fullscreen);
	ScreenModeLabels.Add(FText::FromString(TEXT("전체 화면")));

	ScreenModes.Add(EWindowMode::WindowedFullscreen);
	ScreenModeLabels.Add(FText::FromString(TEXT("테두리 없는 창")));

	ScreenModes.Add(EWindowMode::Windowed);
	ScreenModeLabels.Add(FText::FromString(TEXT("창 모드")));

	// 현재 설정된 모드로 시작 인덱스를 맞춘다.
	ScreenModeIndex = 0;
	if (UGameUserSettings* Settings = GetSettings())
	{
		const EWindowMode::Type Current = Settings->GetFullscreenMode();
		const int32 Found = ScreenModes.IndexOfByKey(Current);
		if (Found != INDEX_NONE)
		{
			ScreenModeIndex = Found;
		}
	}
}

void UGraphicsSettingsWidget::InitResolutions()
{
	Resolutions.Reset();

	// 모니터가 지원하는 전체 화면 해상도 목록을 우선 사용.
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);

	for (const FIntPoint& Res : Supported)
	{
		// 너무 작은 해상도는 제외하고 중복도 거른다.
		if (Res.X >= 1280 && Res.Y >= 720)
		{
			Resolutions.AddUnique(Res);
		}
	}

	// 지원 목록을 못 얻으면 일반적인 16:9 해상도로 대체.
	if (Resolutions.Num() == 0)
	{
		Resolutions.Add(FIntPoint(1280, 720));
		Resolutions.Add(FIntPoint(1600, 900));
		Resolutions.Add(FIntPoint(1920, 1080));
		Resolutions.Add(FIntPoint(2560, 1440));
	}

	// 오름차순 정렬(가로 우선, 같으면 세로).
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return (A.X != B.X) ? (A.X < B.X) : (A.Y < B.Y);
	});

	// 현재 해상도로 시작 인덱스를 맞춘다. 목록에 없으면 추가 후 선택.
	ResolutionIndex = 0;
	if (UGameUserSettings* Settings = GetSettings())
	{
		const FIntPoint Current = Settings->GetScreenResolution();
		int32 Found = Resolutions.IndexOfByKey(Current);
		if (Found == INDEX_NONE && Current.X > 0 && Current.Y > 0)
		{
			Resolutions.Add(Current);
			Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				return (A.X != B.X) ? (A.X < B.X) : (A.Y < B.Y);
			});
			Found = Resolutions.IndexOfByKey(Current);
		}
		if (Found != INDEX_NONE)
		{
			ResolutionIndex = Found;
		}
	}
}

void UGraphicsSettingsWidget::ApplyGraphics()
{
	UGameUserSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	if (ScreenModes.IsValidIndex(ScreenModeIndex))
	{
		Settings->SetFullscreenMode(ScreenModes[ScreenModeIndex]);
	}
	if (Resolutions.IsValidIndex(ResolutionIndex))
	{
		Settings->SetScreenResolution(Resolutions[ResolutionIndex]);
	}

	Settings->ApplyResolutionSettings(false);
	Settings->SaveSettings();
}

void UGraphicsSettingsWidget::RefreshScreenModeText()
{
	if (ScreenModeValueText && ScreenModeLabels.IsValidIndex(ScreenModeIndex))
	{
		ScreenModeValueText->SetText(ScreenModeLabels[ScreenModeIndex]);
	}
}

void UGraphicsSettingsWidget::RefreshResolutionText()
{
	if (ResolutionValueText && Resolutions.IsValidIndex(ResolutionIndex))
	{
		const FIntPoint Res = Resolutions[ResolutionIndex];
		ResolutionValueText->SetText(FText::FromString(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y)));
	}
}

void UGraphicsSettingsWidget::OnScreenModePrev()
{
	if (ScreenModes.Num() == 0) { return; }
	ScreenModeIndex = (ScreenModeIndex - 1 + ScreenModes.Num()) % ScreenModes.Num();
	RefreshScreenModeText();
	ApplyGraphics();
}

void UGraphicsSettingsWidget::OnScreenModeNext()
{
	if (ScreenModes.Num() == 0) { return; }
	ScreenModeIndex = (ScreenModeIndex + 1) % ScreenModes.Num();
	RefreshScreenModeText();
	ApplyGraphics();
}

void UGraphicsSettingsWidget::OnResolutionPrev()
{
	if (Resolutions.Num() == 0) { return; }
	ResolutionIndex = (ResolutionIndex - 1 + Resolutions.Num()) % Resolutions.Num();
	RefreshResolutionText();
	ApplyGraphics();
}

void UGraphicsSettingsWidget::OnResolutionNext()
{
	if (Resolutions.Num() == 0) { return; }
	ResolutionIndex = (ResolutionIndex + 1) % Resolutions.Num();
	RefreshResolutionText();
	ApplyGraphics();
}
