// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShowcaseGameMode.generated.h"

class AOrbitShowcaseCamera;

/**
 * 소개 영상 촬영용 게임모드.
 *
 * 폰을 스폰하지 않고(DefaultPawnClass = nullptr) HUD도 만들지 않는다.
 * 플레이어가 들어오면 레벨에 배치된 AOrbitShowcaseCamera를 찾아 뷰 타깃으로 지정한다.
 *
 * AMainGameMode의 전용 서버/페이즈/네트워크 로직과는 완전히 분리되어 있으므로
 * 이 레벨을 플레이해도 IOCP 서버에 접속을 시도하지 않는다.
 */
UCLASS()
class MANAGER_API AShowcaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShowcaseGameMode();

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
	/**
	 * 이 태그가 비어 있지 않으면 해당 태그를 가진 궤도 카메라만 뷰 타깃으로 삼는다.
	 * 한 레벨에 여러 구도를 배치해 두고 골라 쓸 때 사용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase")
	FName ShowcaseCameraTag = NAME_None;

	/** 카메라가 스트리밍 서브레벨에 있을 때를 대비한 재시도 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase", meta = (ClampMin = "0.05"))
	float CameraBindRetryInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Showcase", meta = (ClampMin = "0"))
	int32 CameraBindMaxRetries = 20;

private:
	AOrbitShowcaseCamera* FindShowcaseCamera() const;

	/** 카메라를 찾아 뷰 타깃으로 지정한다. 아직 없으면 false를 반환한다. */
	bool TryBindShowcaseCamera();

	void RetryBindShowcaseCamera();

	TWeakObjectPtr<APlayerController> PendingViewTargetController;
	FTimerHandle CameraBindRetryTimerHandle;
	int32 CameraBindRetryCount = 0;
};
