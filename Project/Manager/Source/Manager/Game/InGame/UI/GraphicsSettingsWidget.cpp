// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/UI/GraphicsSettingsWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/MainPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

void UGraphicsSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ScreenModeLeftButton)  { ScreenModeLeftButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnScreenModePrev); }
	if (ScreenModeRightButton) { ScreenModeRightButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnScreenModeNext); }
	if (ResolutionLeftButton)  { ResolutionLeftButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnResolutionPrev); }
	if (ResolutionRightButton) { ResolutionRightButton->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::OnResolutionNext); }

	// 현재 설정값으로 인덱스·표시를 맞춘다.
	SyncFromCurrentSettings();
}

void UGraphicsSettingsWidget::SyncFromCurrentSettings()
{
	InitScreenModes();
	InitResolutions();
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

	const EWindowMode::Type Mode = ScreenModes.IsValidIndex(ScreenModeIndex)
		? ScreenModes[ScreenModeIndex].GetValue()
		: EWindowMode::Windowed;

	// 전체화면은 독점(exclusive) 방식으로 강제한다. 그래야 해상도 변경이 실제 디스플레이 모드로 바뀌어
	// 화면을 정확히 꽉 채운다. 윈도우드-풀스크린으로 강제되면 백버퍼가 창보다 커져 잘림/검은 영역이 생긴다.
	// 그 외 모드(보더리스/창)는 0이 아닌 값(윈도우드 풀스크린)으로 둔다.
	if (IConsoleVariable* CVarFSMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FullScreenMode")))
	{
		CVarFSMode->Set(Mode == EWindowMode::Fullscreen ? 0 : 1, ECVF_SetByGameSetting);
	}

	Settings->SetFullscreenMode(Mode);
	if (Resolutions.IsValidIndex(ResolutionIndex))
	{
		Settings->SetScreenResolution(Resolutions[ResolutionIndex]);
	}

	Settings->ApplyResolutionSettings(false);
	// 적용된 화면 모드/해상도를 확정한다(일부 경로의 자동 되돌림 방지).
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();

	// 검증용: 실제로 적용된 값을 로그로 남긴다. (PIE에서는 창이 안 바뀌어도 이 값은 찍힘)
	const FIntPoint AppliedRes = Settings->GetScreenResolution();
	UE_LOG(LogTemp, Warning, TEXT("[Graphics] Applied Mode=%d Res=%dx%d"),
		static_cast<int32>(Settings->GetFullscreenMode()), AppliedRes.X, AppliedRes.Y);

	// 해상도/화면모드 변경은 뷰포트를 재생성하며 입력모드·커서를 리셋할 수 있다.
	// 이 위젯은 ESC 메뉴 안에서만 뜨므로, 변경 후에도 메뉴 입력모드(UI 전용)를 다시 세팅한다.
	// 주의: 여기서 GameAndUI로 바꾸면 게임 뷰포트가 마우스를 잡아 버튼이 연속클릭되므로, 반드시 메뉴 전용 UI모드로 복구한다.
	if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(GetOwningPlayer()))
	{
		MainPC->EnterEscapeMenuInputMode();
	}
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
