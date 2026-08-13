// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EscapeMenuWidget.generated.h"

class UButton;

/**
 * 인게임 ESC 메뉴. [로비로 나가기 / 설정 / 돌아가기] 3개 버튼.
 * 로직만 담당하며, 실제 레이아웃/스타일은 이 클래스를 부모로 하는 WBP에서 구성한다.
 * WBP에는 아래 BindWidget 이름 그대로 버튼을 배치해야 한다:
 *   - ExitToLobbyButton, SettingsButton, ResumeButton
 * 표시/숨김·입력모드는 AMainPlayerController가 관리한다(OpenEscapeMenu/CloseEscapeMenu).
 */
UCLASS()
class MANAGER_API UEscapeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 로비로 나가기: 데디 서버에서 빠져나와 로비(Lobby_Stage)로 복귀.
	UPROPERTY(meta = (BindWidget))
	UButton* ExitToLobbyButton = nullptr;

	// 설정: 이번엔 동작 미구현(자리만 마련). WBP에 없으면 바인딩을 건너뛴다.
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SettingsButton = nullptr;

	// 돌아가기: 메뉴를 닫고 게임으로 복귀.
	UPROPERTY(meta = (BindWidget))
	UButton* ResumeButton = nullptr;

	UFUNCTION()
	void OnExitToLobbyClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnResumeClicked();
};
