// Fill out your copyright notice in the Description page of Project Settings.

#include "Monster/GoldenGoblin/AI/BTT_StopMovement.h"
#include "AIController.h"

UBTT_StopMovement::UBTT_StopMovement() {
	NodeName = TEXT("Stop Movement");
}
EBTNodeResult::Type UBTT_StopMovement::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) {
	AAIController* AIController = Owner.GetAIOwner();
	if (AIController) {
		AIController->StopMovement();
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

void UBTT_StopMovement::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) {

}