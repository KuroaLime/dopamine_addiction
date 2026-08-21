// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShowcasePlayerController.generated.h"

/**
 * 소개 영상 촬영용 플레이어 컨트롤러.
 *
 * 폰을 소유하지 않고, HUD·마우스 커서·입력을 모두 끈 채로
 * AOrbitShowcaseCamera만 바라본다. 화면에 게임 UI가 남지 않으므로
 * 그대로 화면 녹화하면 된다.
 */
UCLASS()
class MANAGER_API AShowcasePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShowcasePlayerController();

	/** 검은 화면에서 페이드 인으로 시작할지 여부. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase")
	bool bFadeInFromBlack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase", meta = (EditCondition = "bFadeInFromBlack", ClampMin = "0.0"))
	float FadeInSeconds = 2.0f;

	/** 모든 입력을 차단해 녹화 중 실수로 화면이 움직이는 것을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase")
	bool bDisableAllInput = true;

protected:
	virtual void BeginPlay() override;
};
