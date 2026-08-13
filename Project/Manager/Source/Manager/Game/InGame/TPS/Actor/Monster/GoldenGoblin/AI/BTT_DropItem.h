// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_DropItem.generated.h"

/**
 * 고블린 처치 보상을 스폰한다. AMainGameMode::SpawnGoldReward를 통해
 * 기존 AGoldDropActor 픽업 파이프라인을 그대로 재사용한다 (Blackboard 키 불필요).
 */
UCLASS()
class MANAGER_API UBTT_DropItem : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
