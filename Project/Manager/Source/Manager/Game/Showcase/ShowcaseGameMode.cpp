// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Showcase/ShowcaseGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Showcase/OrbitShowcaseCamera.h"
#include "Game/Showcase/ShowcasePlayerController.h"
#include "Manager.h"
#include "TimerManager.h"

AShowcaseGameMode::AShowcaseGameMode()
{
	// 플레이어 캐릭터를 스폰하지 않는다. 카메라만 남는다.
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = AShowcasePlayerController::StaticClass();

	bStartPlayersAsSpectators = false;
	bPauseable = false;
}

void AShowcaseGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Super는 RestartPlayer()를 호출해 폰을 스폰하고 PlayerStart를 요구한다.
	// 이 레벨에는 플레이어도 PlayerStart도 없어야 하므로 의도적으로 호출하지 않는다.
	if (!NewPlayer)
	{
		return;
	}

	PendingViewTargetController = NewPlayer;
	CameraBindRetryCount = 0;

	if (TryBindShowcaseCamera())
	{
		return;
	}

	// 카메라가 아직 스트리밍되지 않았을 수 있으므로 짧게 재시도한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CameraBindRetryTimerHandle,
			this,
			&AShowcaseGameMode::RetryBindShowcaseCamera,
			FMath::Max(CameraBindRetryInterval, 0.05f),
			/*bLoop=*/true);
	}
}

AOrbitShowcaseCamera* AShowcaseGameMode::FindShowcaseCamera() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AOrbitShowcaseCamera> It(World); It; ++It)
	{
		AOrbitShowcaseCamera* Camera = *It;
		if (!IsValid(Camera))
		{
			continue;
		}

		if (!ShowcaseCameraTag.IsNone() && !Camera->ActorHasTag(ShowcaseCameraTag))
		{
			continue;
		}

		return Camera;
	}

	return nullptr;
}

bool AShowcaseGameMode::TryBindShowcaseCamera()
{
	APlayerController* PlayerController = PendingViewTargetController.Get();
	if (!PlayerController)
	{
		// 컨트롤러가 사라졌다면 더 시도할 이유가 없다.
		return true;
	}

	AOrbitShowcaseCamera* Camera = FindShowcaseCamera();
	if (!Camera)
	{
		return false;
	}

	PlayerController->SetViewTarget(Camera);

	UE_LOG(LogManager, Log,
		TEXT("[Showcase] 궤도 카메라를 뷰 타깃으로 지정했다. Camera=%s"),
		*Camera->GetName());

	return true;
}

void AShowcaseGameMode::RetryBindShowcaseCamera()
{
	++CameraBindRetryCount;

	const bool bBound = TryBindShowcaseCamera();
	const bool bExhausted = CameraBindRetryCount >= CameraBindMaxRetries;

	if (!bBound && !bExhausted)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CameraBindRetryTimerHandle);
	}

	if (!bBound)
	{
		UE_LOG(LogManager, Warning,
			TEXT("[Showcase] 레벨에서 AOrbitShowcaseCamera를 찾지 못했다(재시도 %d회). ")
			TEXT("레벨에 궤도 카메라를 배치했는지, ShowcaseCameraTag(%s)가 맞는지 확인할 것."),
			CameraBindRetryCount,
			*ShowcaseCameraTag.ToString());
	}
}
