// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "GoldenGoblinAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;

/**
 * 황금 고블린 전용 AIController.
 * Sight Perception으로 플레이어를 감지해 Blackboard(TargetActor)를 갱신하고,
 * 고블린 사망 시 Blackboard(IsDead)를 세팅해 Behavior Tree의 최우선 브랜치를 트리거한다.
 */
UCLASS()
class MANAGER_API AGoldenGoblinAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGoldenGoblinAIController();

	// Blackboard 에셋(BehaviorTreeAsset->BlackboardAsset)에 반드시 아래 이름의 키가 있어야 한다:
	// TargetActor(Object), IsDead(Bool), PatrolLocation(Vector), EscapeLocation(Vector)
	static const FName TargetActorKey;
	static const FName IsDeadKey;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void HandleGoblinDied();

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float PeripheralVisionAngleDegrees = 90.f;

private:
	TWeakObjectPtr<class AGoldenGoblinCharacter> PossessedGoblin;
};
