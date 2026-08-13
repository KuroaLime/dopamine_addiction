// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/AI/Patrol/BTT_GetRandomLocation.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTT_GetRandomLocation::UBTT_GetRandomLocation()
{
	NodeName = TEXT("Get Random Patrol Location");
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_GetRandomLocation, BlackboardKey));
}

EBTNodeResult::Type UBTT_GetRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* AIPawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!AIPawn || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Evade 브랜치에서 부스트된 속도를 배회 속도로 되돌린다(Patrol 브랜치 진입 지점).
	if (AGoldenGoblinCharacter* Goblin = Cast<AGoldenGoblinCharacter>(AIPawn))
	{
		Goblin->SetGoblinMoveSpeed(Goblin->GetPatrolMoveSpeed());
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(AIPawn->GetWorld());
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation ResultLocation;
	if (NavSys->GetRandomReachablePointInRadius(AIPawn->GetActorLocation(), SearchRadius, ResultLocation))
	{
		BlackboardComp->SetValueAsVector(BlackboardKey.SelectedKeyName, ResultLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

