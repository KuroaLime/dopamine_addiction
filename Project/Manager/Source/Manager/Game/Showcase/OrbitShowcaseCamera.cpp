// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Showcase/OrbitShowcaseCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "Manager.h"

AOrbitShowcaseCamera::AOrbitShowcaseCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	PivotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PivotRoot"));
	SetRootComponent(PivotRoot);

	OrbitArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OrbitArm"));
	OrbitArm->SetupAttachment(PivotRoot);
	OrbitArm->TargetArmLength = OrbitRadius;
	// 지형을 통과해 도는 궤도이므로 충돌로 인한 줌인은 반드시 꺼야 한다.
	OrbitArm->bDoCollisionTest = false;
	OrbitArm->bUsePawnControlRotation = false;
	OrbitArm->bInheritPitch = false;
	OrbitArm->bInheritYaw = false;
	OrbitArm->bInheritRoll = false;
	// 스프링암 러그는 등속 회전에서 지연만 만들고 이득이 없다.
	OrbitArm->bEnableCameraLag = false;
	OrbitArm->bEnableCameraRotationLag = false;

	ShowcaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ShowcaseCamera"));
	ShowcaseCamera->SetupAttachment(OrbitArm, USpringArmComponent::SocketName);
	ShowcaseCamera->bUsePawnControlRotation = false;
	ShowcaseCamera->SetFieldOfView(FieldOfView);
}

void AOrbitShowcaseCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터 뷰포트에서 설정을 바꾸는 즉시 구도를 확인할 수 있게 시작 방위각으로 미리 그려준다.
	CurrentAzimuthDegrees = StartAzimuthDegrees;
	ElapsedSeconds = 0.0f;
	ApplyCameraTransform();
}

void AOrbitShowcaseCamera::BeginPlay()
{
	Super::BeginPlay();

	CurrentAzimuthDegrees = StartAzimuthDegrees;
	ElapsedSeconds = 0.0f;

	if (bAutoFrameOnBeginPlay)
	{
		FrameLevelNow();
	}

	ApplyCameraTransform();
}

void AOrbitShowcaseCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Direction = bReverseOrbit ? -1.0f : 1.0f;
	CurrentAzimuthDegrees = FMath::Fmod(
		CurrentAzimuthDegrees + Direction * OrbitSpeedDegreesPerSecond * DeltaSeconds,
		360.0f);
	ElapsedSeconds += DeltaSeconds;

	ApplyCameraTransform();
}

void AOrbitShowcaseCamera::ApplyCameraTransform()
{
	if (!OrbitArm || !ShowcaseCamera)
	{
		return;
	}

	float Height = OrbitHeightOffset;
	if (bEnableHeightBob && HeightBobPeriodSeconds > KINDA_SMALL_NUMBER)
	{
		const float Phase = 2.0f * PI * ElapsedSeconds / HeightBobPeriodSeconds;
		Height += HeightBobAmplitude * FMath::Sin(Phase);
	}

	float Radius = OrbitRadius;
	if (bEnableRadiusPulse && RadiusPulsePeriodSeconds > KINDA_SMALL_NUMBER)
	{
		const float Phase = 2.0f * PI * ElapsedSeconds / RadiusPulsePeriodSeconds;
		Radius += RadiusPulseAmplitude * FMath::Sin(Phase);
	}

	OrbitArm->SetRelativeLocation(FVector(0.0f, 0.0f, Height));
	OrbitArm->SetRelativeRotation(FRotator(-OrbitPitchDegrees, CurrentAzimuthDegrees, 0.0f));
	OrbitArm->TargetArmLength = FMath::Max(Radius, 1.0f);

	ShowcaseCamera->SetFieldOfView(FieldOfView);
}

void AOrbitShowcaseCamera::FrameLevelNow()
{
	FBox LevelBounds(ForceInit);
	if (!ComputeLevelBounds(LevelBounds))
	{
		UE_LOG(LogManager, Warning,
			TEXT("[Showcase] 레벨 바운드를 계산하지 못해 자동 구도 맞춤을 건너뛴다. 수동 OrbitRadius(%.0f)를 사용한다."),
			OrbitRadius);
		return;
	}

	// 디테일 패널 버튼으로 누른 경우, 바뀐 위치와 반경이 저장 대상으로 잡히고
	// Ctrl+Z로 되돌려지도록 트랜잭션에 등록한다. 런타임 자동 맞춤에는 불필요하다.
#if WITH_EDITOR
	if (const UWorld* World = GetWorld(); World && !World->IsGameWorld())
	{
		Modify();
	}
#endif

	const FVector Pivot = LevelBounds.GetCenter() + FVector(0.0f, 0.0f, FramingPivotZOffset);
	SetActorLocation(Pivot);

	// 바운드를 감싸는 구가 화각 안에 들어오는 거리를 구한다.
	const float BoundingSphereRadius = LevelBounds.GetExtent().Size();
	const float HalfFOVRadians = FMath::DegreesToRadians(FMath::Clamp(FieldOfView, 10.0f, 170.0f) * 0.5f);
	const float FitDistance = BoundingSphereRadius / FMath::Max(FMath::Tan(HalfFOVRadians), KINDA_SMALL_NUMBER);

	OrbitRadius = FMath::Max(FitDistance * FramingPadding, 100.0f);

	UE_LOG(LogManager, Log,
		TEXT("[Showcase] 자동 구도 맞춤 완료. Pivot=(%.0f, %.0f, %.0f) BoundsRadius=%.0f OrbitRadius=%.0f"),
		Pivot.X, Pivot.Y, Pivot.Z, BoundingSphereRadius, OrbitRadius);

	ApplyCameraTransform();
}

bool AOrbitShowcaseCamera::ComputeLevelBounds(FBox& OutBounds) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	OutBounds.Init();
	bool bFoundAny = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		// 플레이어 스타트는 섬 밖에 놓이는 경우가 있어 구도를 왜곡시킨다.
		if (Actor->IsA<APlayerStart>())
		{
			continue;
		}

		if (!FramingActorTag.IsNone() && !Actor->ActorHasTag(FramingActorTag))
		{
			continue;
		}

		FBox ActorBounds(ForceInit);
		bool bActorHasVisiblePrimitive = false;

		Actor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/true,
			[this, &ActorBounds, &bActorHasVisiblePrimitive](const UPrimitiveComponent* Primitive)
			{
				if (!Primitive || !Primitive->IsRegistered())
				{
					return;
				}

				// 게임 화면에 보이지 않는 것(볼륨·트리거·숨김 메시)은 구도에 포함하지 않는다.
				if (!Primitive->IsVisible() || Primitive->bHiddenInGame)
				{
					return;
				}

				const FBoxSphereBounds PrimitiveBounds = Primitive->Bounds;

				// 스카이 스피어/대기 액터처럼 월드 전체를 감싸는 프리미티브는 제외한다.
				if (PrimitiveBounds.SphereRadius > MaxPrimitiveRadiusForFraming)
				{
					return;
				}

				ActorBounds += PrimitiveBounds.GetBox();
				bActorHasVisiblePrimitive = true;
			});

		if (bActorHasVisiblePrimitive)
		{
			OutBounds += ActorBounds;
			bFoundAny = true;
		}
	}

	return bFoundAny && OutBounds.IsValid != 0;
}
