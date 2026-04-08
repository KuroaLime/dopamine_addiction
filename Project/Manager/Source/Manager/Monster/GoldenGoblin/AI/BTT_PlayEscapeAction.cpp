// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster/GoldenGoblin/AI/BTT_PlayEscapeAction.h"
#include "AIController.h"
#include "GameFramework/Character.h"

UBTT_PlayEscapeAction::UBTT_PlayEscapeAction() {
	NodeName = TEXT("Play EscapeAction");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_PlayEscapeAction::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) {
	ACharacter* AICharacter = Cast<ACharacter>(Owner.GetAIOwner()->GetPawn());

	if (AICharacter && EscapeMontage) {
		AICharacter->PlayAnimMontage(EscapeMontage);

		return EBTNodeResult::InProgress;
	}
	return EBTNodeResult::Failed;
}

void UBTT_PlayEscapeAction::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) {
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	ACharacter* AICharacter = Cast<ACharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (AICharacter && EscapeMontage) {
		UAnimInstance* AnimInstance = AICharacter->GetMesh()->GetAnimInstance();

		if (AnimInstance && !AnimInstance->Montage_IsPlaying(EscapeMontage)) {
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}