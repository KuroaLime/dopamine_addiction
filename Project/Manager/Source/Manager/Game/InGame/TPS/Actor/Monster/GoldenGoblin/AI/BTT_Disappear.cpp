// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/AI/BTT_Disappear.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTT_Disappear::UBTT_Disappear() {
	NodeName = TEXT("Disappear Actor");
}

EBTNodeResult::Type UBTT_Disappear::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory) {
	APawn* AIPawn = Owner.GetAIOwner()->GetPawn();
	UE_LOG(LogTemp, Warning, TEXT("[Goblin] BTT_Disappear executing for %s, about to Destroy()"), *GetNameSafe(AIPawn));
	if (AIPawn) {
		if (DisapperedEffect)
		{
			// SpawnSystemAtLocation은 리플리케이트되지 않는 로컬 전용 호출이라 서버(AI가 도는 곳)에서
			// 직접 부르면 어떤 클라이언트에도 보이지 않는다. 캐릭터의 Multicast RPC를 거쳐야 한다.
			if (AGoldenGoblinCharacter* Goblin = Cast<AGoldenGoblinCharacter>(AIPawn))
			{
				Goblin->Multicast_PlayDeathEffect(DisapperedEffect, AIPawn->GetActorLocation());
			}
		}
		AIPawn->Destroy();

		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

void UBTT_Disappear::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) {

}