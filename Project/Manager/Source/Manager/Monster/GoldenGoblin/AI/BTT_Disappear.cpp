// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster/GoldenGoblin/AI/BTT_Disappear.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NiagaraFunctionLibrary.h"

UBTT_Disappear::UBTT_Disappear() {
	NodeName = TEXT("Disappear Actor");
}

EBTNodeResult::Type UBTT_Disappear::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) {
	APawn* AIPawn = Owner.GetAIOwner()->GetPawn();
	if (AIPawn) {
		if (DisapperedEffect)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DisapperedEffect, AIPawn->GetActorLocation());
		AIPawn->Destroy();

		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

void UBTT_Disappear::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) {

}