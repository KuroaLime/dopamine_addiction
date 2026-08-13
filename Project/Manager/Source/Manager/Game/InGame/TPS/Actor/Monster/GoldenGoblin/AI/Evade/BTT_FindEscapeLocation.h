// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_FindEscapeLocation.generated.h"

/**
 * TargetActorKey가 가리키는 위협 반대 방향으로 NavMesh 도달 가능한 지점을 찾아
 * BlackboardKey(Vector)에 기록한다. BT에서 BlackboardKey를 EscapeLocation으로 설정해서 사용.
 */
UCLASS()
class MANAGER_API UBTT_FindEscapeLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTT_FindEscapeLocation();

	// 도주 방향으로 얼마나 멀리 떨어진 지점을 기준으로 탐색할지.
	UPROPERTY(EditAnywhere, Category = "Evade", meta = (ClampMin = "0.0"))
	float EscapeDistance = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Evade", meta = (ClampMin = "0.0"))
	float SearchRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Evade", meta = (ClampMin = "1"))
	int32 MaxAttempts = 5;

protected:
	// 시야를 준 위협(플레이어)을 읽어오는 키. 출력은 상속받은 BlackboardKey(Vector)를 사용.
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
