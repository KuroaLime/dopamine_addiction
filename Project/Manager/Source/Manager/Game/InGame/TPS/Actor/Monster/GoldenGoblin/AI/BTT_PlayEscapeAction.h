// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_PlayEscapeAction.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UBTT_PlayEscapeAction : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
public:
	UBTT_PlayEscapeAction();

	UPROPERTY(EditAnywhere, Category = "Animation")
	class UAnimMontage* EscapeMontage;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
