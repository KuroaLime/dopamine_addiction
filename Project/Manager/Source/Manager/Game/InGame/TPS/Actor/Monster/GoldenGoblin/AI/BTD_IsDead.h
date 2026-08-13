// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BTD_IsDead.generated.h"

/**
 * BlackboardKey(Bool, IsDead)의 실제 값을 확인한다. 네이티브 "Blackboard" 데코레이터가
 * 이 프로젝트 환경에서 저장 시 키가 되돌아가는 현상이 있어 커스텀으로 대체.
 *
 * UBTDecorator_BlackboardBase가 아니라 UBTDecorator_Blackboard를 상속해야 한다 —
 * Observer Aborts(값이 바뀌는 즉시 BT에 재평가를 요청하는 기능)를 실제로 동작시키는
 * Blackboard 옵저버 등록 로직이 Base가 아니라 여기(Blackboard)에 구현되어 있다.
 * 평가 로직(CalculateRawConditionValue)도 베이스가 Bool 키에 대해 이미 올바르게
 * 처리하므로 따로 오버라이드하지 않는다.
 */
UCLASS()
class MANAGER_API UBTD_IsDead : public UBTDecorator_Blackboard
{
	GENERATED_BODY()

public:
	UBTD_IsDead();
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	// 디버깅용: Super의 판정 결과를 그대로 쓰되 언제/어떻게 평가되는지 로그로 남긴다.
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
