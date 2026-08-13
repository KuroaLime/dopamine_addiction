// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/AI/BTT_DropItem.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "Game/InGame/MainGameMode.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

EBTNodeResult::Type UBTT_DropItem::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AGoldenGoblinCharacter* Goblin = AIController ? Cast<AGoldenGoblinCharacter>(AIController->GetPawn()) : nullptr;
	if (!Goblin)
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] BTT_DropItem: no possessed GoldenGoblinCharacter, failing"));
		return EBTNodeResult::Failed;
	}

	AMainGameMode* GameMode = Goblin->GetWorld() ? Goblin->GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] BTT_DropItem: GetAuthGameMode<AMainGameMode> is NULL, failing"));
		return EBTNodeResult::Failed;
	}

	const bool bSpawned = GameMode->SpawnGoldReward(Goblin->GetGoldRewardAmount(), Goblin->GetActorLocation(), Goblin);
	UE_LOG(LogTemp, Warning, TEXT("[Goblin] BTT_DropItem: SpawnGoldReward returned %s (Succeeded regardless)"),
		bSpawned ? TEXT("true") : TEXT("false"));
	return EBTNodeResult::Succeeded;
}

