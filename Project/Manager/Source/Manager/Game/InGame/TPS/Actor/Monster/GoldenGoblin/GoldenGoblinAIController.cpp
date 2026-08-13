// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinAIController.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AIPerceptionTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

const FName AGoldenGoblinAIController::TargetActorKey(TEXT("TargetActor"));
const FName AGoldenGoblinAIController::IsDeadKey(TEXT("IsDead"));

AGoldenGoblinAIController::AGoldenGoblinAIController()
{
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGoldenGoblinAIController::OnTargetPerceptionUpdated);

	SetPerceptionComponent(*AIPerceptionComp);
}

void AGoldenGoblinAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	PossessedGoblin = Cast<AGoldenGoblinCharacter>(InPawn);
	if (AGoldenGoblinCharacter* Goblin = PossessedGoblin.Get())
	{
		Goblin->OnGoblinDied.AddUObject(this, &AGoldenGoblinAIController::HandleGoblinDied);
		UE_LOG(LogTemp, Warning, TEXT("[Goblin] AIController OnPossess: bound to OnGoblinDied"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] AIController OnPossess: possessed pawn is NOT a GoldenGoblinCharacter (%s)"), *GetNameSafe(InPawn));
	}

	if (BehaviorTreeAsset)
	{
		const bool bStarted = RunBehaviorTree(BehaviorTreeAsset);
		UE_LOG(LogTemp, Warning, TEXT("[Goblin] AIController RunBehaviorTree started=%d"), bStarted);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] AIController OnPossess: BehaviorTreeAsset is NULL"));
	}
}

void AGoldenGoblinAIController::OnUnPossess()
{
	if (AGoldenGoblinCharacter* Goblin = PossessedGoblin.Get())
	{
		Goblin->OnGoblinDied.RemoveAll(this);
	}
	PossessedGoblin = nullptr;

	Super::OnUnPossess();
}

void AGoldenGoblinAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Blackboard || !Actor)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		Blackboard->SetValueAsObject(TargetActorKey, Actor);
	}
	else if (Blackboard->GetValueAsObject(TargetActorKey) == Actor)
	{
		// 시야를 잃은 대상이 현재 추적 중이던 타겟이면 해제 → BT가 Patrol 브랜치로 복귀.
		Blackboard->ClearValue(TargetActorKey);
	}
}

void AGoldenGoblinAIController::HandleGoblinDied()
{
	UE_LOG(LogTemp, Warning, TEXT("[Goblin] AIController HandleGoblinDied called, Blackboard=%p"), Blackboard.Get());

	if (Blackboard)
	{
		Blackboard->SetValueAsBool(IsDeadKey, true);
		UE_LOG(LogTemp, Warning, TEXT("[Goblin] AIController wrote IsDead=true, readback GetValueAsBool(IsDead)=%s"),
			Blackboard->GetValueAsBool(IsDeadKey) ? TEXT("true") : TEXT("false"));
	}
}
