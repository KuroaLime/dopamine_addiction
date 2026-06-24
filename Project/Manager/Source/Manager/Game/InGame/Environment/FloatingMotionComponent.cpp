// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/Environment/FloatingMotionComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UFloatingMotionComponent::UFloatingMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UFloatingMotionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFloatingMotionComponent, RepZOffset);
}

void UFloatingMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// 시작(휴지) 위치를 기준으로 삼는다. 이후 모든 오프셋은 이 기준 대비.
	BaseLocation = Owner->GetActorLocation();
	IdlePhase = FMath::FRandRange(0.f, 2.f * PI);
	bInitialized = true;

	if (Owner->HasAuthority())
	{
		// 멀티: 섬과 이 컴포넌트가 클라로 복제되도록. (확실하게 하려면 BP 기본값에서도 Replicates 체크)
		Owner->SetReplicates(true);

		// 첫 구간은 잠깐 정지부터 시작 (섬마다 위상이 자연스레 어긋남).
		bMoving = false;
		SegTimeLeft = FMath::FRandRange(FMath::Min(PauseTimeMin, PauseTimeMax), FMath::Max(PauseTimeMin, PauseTimeMax));
	}
}

void UFloatingMotionComponent::StartNextSegment()
{
	if (bMoving)
	{
		// 이동 끝 → 정지
		bMoving = false;
		SegTimeLeft = FMath::FRandRange(FMath::Min(PauseTimeMin, PauseTimeMax), FMath::Max(PauseTimeMin, PauseTimeMax));
	}
	else
	{
		// 정지 끝 → 새 이동. 목표는 기준 대비 절대값(누적 X) → 드리프트 없음.
		bMoving = true;
		MoveFromZ = RepZOffset;
		MoveToZ = FMath::FRandRange(-HeightRange, HeightRange);
		MoveDuration = FMath::FRandRange(FMath::Min(MoveTimeMin, MoveTimeMax), FMath::Max(MoveTimeMin, MoveTimeMax));
		MoveElapsed = 0.f;
	}
}

void UFloatingMotionComponent::ServerUpdate(float Dt)
{
	if (bMoving)
	{
		MoveElapsed += Dt;
		const float Alpha = (MoveDuration > 0.f) ? FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f) : 1.f;
		const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha); // 가감속(ease in-out)
		RepZOffset = FMath::Lerp(MoveFromZ, MoveToZ, Eased);

		if (Alpha >= 1.f)
		{
			StartNextSegment();
		}
	}
	else
	{
		SegTimeLeft -= Dt;
		if (SegTimeLeft <= 0.f)
		{
			StartNextSegment();
		}
	}
}

void UFloatingMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialized) return;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;

	if (Owner->HasAuthority())
	{
		// 서버: 상태머신이 큰-모션을 직접 계산.
		ServerUpdate(DeltaTime);
		DisplayZ = RepZOffset;
	}
	else
	{
		// 클라: 복제값(네트 주기로 띄엄띄엄 옴)을 향해 매 프레임 부드럽게 보간.
		DisplayZ = FMath::FInterpTo(DisplayZ, RepZOffset, DeltaTime, 6.f);
	}

	// 상시 미세 보브 (로컬·코스메틱 — 작아서 머신 간 위상차 무의미). 정지 중에도 "살아있게".
	const float T = World->GetTimeSeconds();
	const float IdleBob = (IdleBobPeriod > 0.f)
		? IdleBobAmplitude * FMath::Sin(T * (2.f * PI / IdleBobPeriod) + IdlePhase)
		: 0.f;

	// Z 상하 이동만 (회전 없음). 섬 위 플레이어는 CharacterMovement의 Based Movement로 함께 따라온다(루트 Movable + 콜리전 필요).
	const FVector NewLocation = BaseLocation + FVector(0.f, 0.f, DisplayZ + IdleBob);
	Owner->SetActorLocation(NewLocation);
}
