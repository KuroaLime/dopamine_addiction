// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EscapeMenuWidget.generated.h"

class UButton;
class UWidgetSwitcher;

/**
 * 인게임 ESC 메뉴. 좌측 탭(그래픽/소리/…) + 우측 설정 콘텐츠 + 하단 2버튼(로비로 나가기/돌아가기).
 * 로직만 담당하며 레이아웃/스타일은 이 클래스를 부모로 하는 WBP(WBP_EscapeMenu)에서 구성한다.
 *
 * 버튼의 3상태(디폴트/호버/프레스)는 각 UButton의 Style(Normal/Hovered/Pressed 브러시)에서 지정한다 — 코드 불필요.
 *
 * WBP에 아래 이름으로 위젯을 배치한다(탭/스위처는 선택):
 *   - 탭:      GraphicsTab, SoundTab, ThirdTab   (UButton)
 *   - 콘텐츠:  ContentSwitcher                    (UWidgetSwitcher; 탭 순서와 인덱스 일치)
 *   - 하단:    ExitToLobbyButton(로비로 나가기), ResumeButton(돌아가기)  (UButton)
 *
 * 표시/숨김·입력모드는 AMainPlayerController가 관리한다(OpenEscapeMenu/CloseEscapeMenu).
 */
UCLASS()
class MANAGER_API UEscapeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// --- 좌측 탭 (선택: 없으면 건너뜀) ---
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* GraphicsTab = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SoundTab = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ThirdTab = nullptr;

	// 탭 순서와 인덱스가 일치하는 콘텐츠 스위처(선택).
	UPROPERTY(meta = (BindWidgetOptional))
	UWidgetSwitcher* ContentSwitcher = nullptr;

	// --- 하단 버튼 ---
	UPROPERTY(meta = (BindWidget))
	UButton* ExitToLobbyButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* ResumeButton = nullptr;

	UFUNCTION()
	void OnGraphicsTabClicked();

	UFUNCTION()
	void OnSoundTabClicked();

	UFUNCTION()
	void OnThirdTabClicked();

	UFUNCTION()
	void OnExitToLobbyClicked();

	UFUNCTION()
	void OnResumeClicked();

private:
	// 콘텐츠 스위처를 해당 인덱스로 전환한다(스위처가 없거나 범위를 벗어나면 무시).
	void ShowContentIndex(int32 Index);
};
