// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_PatrolMoveTo.generated.h"

/**
 * BlackboardKey(Vector, PatrolLocation)로 배회 속도로 이동한다.
 * 목적지 도달 또는 경로 실패 시까지 InProgress로 대기하는 Latent Task.
 * 엔진 내장 MoveTo 대신 사용 — Evasive Maneuver와 동일한 패턴.
 */
UCLASS()
class MANAGER_API UBTT_PatrolMoveTo : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTT_PatrolMoveTo();

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 80.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
