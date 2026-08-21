// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Showcase/ShowcasePlayerController.h"

#include "Camera/PlayerCameraManager.h"

AShowcasePlayerController::AShowcasePlayerController()
{
	PrimaryActorTick.bCanEverTick = false;

	bShowMouseCursor = false;
	// 폰이 없으므로 엔진이 뷰 타깃을 임의로 되돌리지 않도록 자동 관리를 끈다.
	bAutoManageActiveCameraTarget = false;
}

void AShowcasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	// HUD·이동·회전 입력을 한 번에 차단한다. 화면에 게임 UI가 남지 않는다.
	SetCinematicMode(
		/*bInCinematicMode=*/true,
		/*bHidePlayer=*/true,
		/*bAffectsHUD=*/true,
		/*bAffectsMovement=*/true,
		/*bAffectsTurning=*/true);

	if (bDisableAllInput)
	{
		DisableInput(this);
	}

	if (bFadeInFromBlack && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(
			/*FromAlpha=*/1.0f,
			/*ToAlpha=*/0.0f,
			FMath::Max(FadeInSeconds, 0.0f),
			FLinearColor::Black,
			/*bShouldFadeAudio=*/false,
			/*bHoldWhenFinished=*/false);
	}
}
