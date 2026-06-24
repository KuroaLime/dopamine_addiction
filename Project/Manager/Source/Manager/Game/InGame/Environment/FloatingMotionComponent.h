// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FloatingMotionComponent.generated.h"

/**
 * 공중 섬 부유 컴포넌트 (Z 상하 이동만, 회전 없음).
 * "랜덤 시간 동안 랜덤 높이로 이동 → 랜덤 시간 정지 → 반복" + 정지 중에도 살아있게 하는 상시 미세 보브.
 *
 * - 드리프트 방지: 목표 높이는 항상 기준 위치 대비 ±HeightRange "절대값"으로 뽑음(누적 아님).
 * - 부드러움: 목표까지 SmoothStep으로 가감속.
 * - 멀티: 서버가 모션을 계산해 오프셋을 복제, 클라는 부드럽게 따라감(서버 권위).
 *
 * 사용: 섬 BPP에 이 컴포넌트 추가. 섬 루트 Mobility = Movable, (멀티면) BP에서 Replicates 체크 권장.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MANAGER_API UFloatingMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFloatingMotionComponent();

	// === 이동 (랜덤 이동/정지) ===

	// 기준 높이에서 위아래로 허용되는 최대 폭(cm). 목표는 이 범위 안에서만 뽑혀 드리프트가 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Move", meta = (ClampMin = "0.0"))
	float HeightRange = 50.f;

	// 한 번 이동에 걸리는 시간(초) 최소/최대.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Move", meta = (ClampMin = "0.1"))
	float MoveTimeMin = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Move", meta = (ClampMin = "0.1"))
	float MoveTimeMax = 5.f;

	// 이동 후 멈춰 있는 시간(초) 최소/최대.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Move", meta = (ClampMin = "0.0"))
	float PauseTimeMin = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Move", meta = (ClampMin = "0.0"))
	float PauseTimeMax = 3.f;

	// === 상시 미세 보브 (정지 중에도 "얼지" 않게) ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Idle", meta = (ClampMin = "0.0"))
	float IdleBobAmplitude = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floating|Idle", meta = (ClampMin = "0.1"))
	float IdleBobPeriod = 2.5f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// 서버가 계산한 큰-모션 Z 오프셋 (복제). 클라는 이 값으로 부드럽게 보간.
	UPROPERTY(Replicated)
	float RepZOffset = 0.f;

	FVector BaseLocation = FVector::ZeroVector;
	bool bInitialized = false;
	float IdlePhase = 0.f;

	// 서버 상태머신
	bool bMoving = false;
	float SegTimeLeft = 0.f;        // 정지 구간 남은 시간
	float MoveFromZ = 0.f;
	float MoveToZ = 0.f;
	float MoveDuration = 0.f;
	float MoveElapsed = 0.f;

	// 클라 표시용 부드러운 보간값
	float DisplayZ = 0.f;

	void ServerUpdate(float Dt);
	void StartNextSegment();
};
