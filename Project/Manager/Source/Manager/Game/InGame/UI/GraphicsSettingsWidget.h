// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GraphicsSettingsWidget.generated.h"

class UButton;
class UTextBlock;
class UGameUserSettings;

/**
 * 그래픽 설정 콘텐츠 위젯. ContentSwitcher의 그래픽 페이지(인덱스 0)에 넣는다.
 * 두 가지 기능을 [라벨] ← 값 → 형태의 셀렉터로 제공한다:
 *   - 화면 모드: 전체 화면 / 테두리 없는 창 / 창 모드
 *   - 해상도  : 지원 해상도 목록을 순환
 *
 * 값 변경은 UGameUserSettings로 즉시 적용·저장된다(로컬 렌더링 설정이라 네트워크와 무관).
 * (PIE보다 Standalone/패키지 빌드에서 정확히 반영된다.)
 *
 * WBP(WBP_GraphicsSettings, 부모=이 클래스)에 아래 이름으로 배치한다:
 *   화면 모드: ScreenModeLeftButton(←), ScreenModeValueText, ScreenModeRightButton(→)
 *   해상도  : ResolutionLeftButton(←), ResolutionValueText, ResolutionRightButton(→)
 */
UCLASS()
class MANAGER_API UGraphicsSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 현재 UGameUserSettings 값으로 인덱스·표시 텍스트를 다시 맞춘다.
	// 메뉴를 다시 열 때(위젯이 재사용될 때) 상태가 낡지 않도록 외부에서 호출한다.
	void SyncFromCurrentSettings();

protected:
	virtual void NativeConstruct() override;

	// --- 화면 모드 ---
	UPROPERTY(meta = (BindWidget))
	UButton* ScreenModeLeftButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* ScreenModeRightButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScreenModeValueText = nullptr;

	// --- 해상도 ---
	UPROPERTY(meta = (BindWidget))
	UButton* ResolutionLeftButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* ResolutionRightButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ResolutionValueText = nullptr;

	UFUNCTION()
	void OnScreenModePrev();

	UFUNCTION()
	void OnScreenModeNext();

	UFUNCTION()
	void OnResolutionPrev();

	UFUNCTION()
	void OnResolutionNext();

private:
	// 화면 모드 목록과 표시 라벨(인덱스 대응).
	TArray<TEnumAsByte<EWindowMode::Type>> ScreenModes;
	TArray<FText> ScreenModeLabels;
	int32 ScreenModeIndex = 0;

	// 순환 가능한 해상도 목록.
	TArray<FIntPoint> Resolutions;
	int32 ResolutionIndex = 0;

	void InitScreenModes();
	void InitResolutions();

	// 현재 인덱스의 화면 모드/해상도를 실제 적용하고 저장한다.
	void ApplyGraphics();

	void RefreshScreenModeText();
	void RefreshResolutionText();

	UGameUserSettings* GetSettings() const;
};
