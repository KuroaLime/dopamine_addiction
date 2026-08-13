// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/AI/BTD_IsDead.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AIController.h"

UBTD_IsDead::UBTD_IsDead()
{
	NodeName = TEXT("Is Goblin Dead");
	BlackboardKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTD_IsDead, BlackboardKey));
}

void UBTD_IsDead::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// SelectedKeyID가 InvalidKey(65535)로 굳어 저장되는 현상이 있어, 에셋을 받는 시점에
	// 이름(SelectedKeyName) 기준으로 강제로 재resolve한다.
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		BlackboardKey.ResolveSelectedKey(*BBAsset);
		UE_LOG(LogTemp, Warning, TEXT("[Goblin] BTD_IsDead::InitializeFromAsset resolved KeyName=%s -> KeyID=%d"),
			*BlackboardKey.SelectedKeyName.ToString(),
			static_cast<int32>(BlackboardKey.GetSelectedKeyID()));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] BTD_IsDead::InitializeFromAsset: GetBlackboardAsset() is NULL"));
	}
}

bool UBTD_IsDead::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const bool bResult = Super::CalculateRawConditionValue(OwnerComp, NodeMemory);

	const UBlackboardComponent* BBComp = OwnerComp.GetBlackboardComponent();
	const bool bByName = BBComp ? BBComp->GetValueAsBool(BlackboardKey.SelectedKeyName) : false;
	UE_LOG(LogTemp, Warning,
		TEXT("[Goblin] BTD_IsDead evaluated for %s -> %s | BBComp=%p KeyName=%s KeyID=%d ByNameRead=%s"),
		*GetNameSafe(OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr),
		bResult ? TEXT("true") : TEXT("false"),
		BBComp,
		*BlackboardKey.SelectedKeyName.ToString(),
		static_cast<int32>(BlackboardKey.GetSelectedKeyID()),
		bByName ? TEXT("true") : TEXT("false"));
	return bResult;
}
