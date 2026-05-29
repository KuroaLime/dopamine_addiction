// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_Disappear.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UBTT_Disappear : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
public:
	UBTT_Disappear();

	UPROPERTY(EditAnywhere, Category = "VFX")
	class UNiagaraSystem* DisapperedEffect;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
