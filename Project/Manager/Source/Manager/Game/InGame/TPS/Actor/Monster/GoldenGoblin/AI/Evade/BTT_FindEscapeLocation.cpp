// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/AI/Evade/BTT_FindEscapeLocation.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTT_FindEscapeLocation::UBTT_FindEscapeLocation()
{
	NodeName = TEXT("Find Escape Location");
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_FindEscapeLocation, BlackboardKey));
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_FindEscapeLocation, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTT_FindEscapeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* AIPawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!AIPawn || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	const FVector SelfLocation = AIPawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float CurrentDistSq = FVector::DistSquared(SelfLocation, TargetLocation);

	FVector AwayDirection = (SelfLocation - TargetLocation).GetSafeNormal();
	if (AwayDirection.IsNearlyZero())
	{
		AwayDirection = AIPawn->GetActorForwardVector();
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(AIPawn->GetWorld());
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	const FVector SearchOrigin = SelfLocation + AwayDirection * EscapeDistance;

	// 위협 반대편 기준점 주변에서 후보를 뽑되, 실제로 타겟에서 더 멀어지는 지점만 채택한다.
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FNavLocation ResultLocation;
		if (NavSys->GetRandomReachablePointInRadius(SearchOrigin, SearchRadius, ResultLocation))
		{
			if (FVector::DistSquared(ResultLocation.Location, TargetLocation) >= CurrentDistSq)
			{
				BlackboardComp->SetValueAsVector(BlackboardKey.SelectedKeyName, ResultLocation.Location);
				return EBTNodeResult::Succeeded;
			}
		}
	}

	return EBTNodeResult::Failed;
}

