// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_GetRandomLocation.generated.h"

/**
 * 자기 위치 기준 반경 내 NavMesh 도달 가능한 랜덤 지점을 찾아 BlackboardKey(Vector)에 기록한다.
 * BT에서 BlackboardKey를 PatrolLocation으로 설정해서 사용.
 */
UCLASS()
class MANAGER_API UBTT_GetRandomLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTT_GetRandomLocation();

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float SearchRadius = 1500.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
