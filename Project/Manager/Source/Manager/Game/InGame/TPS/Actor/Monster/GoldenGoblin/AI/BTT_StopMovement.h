// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_StopMovement.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UBTT_StopMovement : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
public:
	UBTT_StopMovement();
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
