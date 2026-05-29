// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "GameplayTagContainer.h"
#include "CustomAbility.generated.h"

/**
 * 
 */

class UCustomASC;
class AActor;
class ACharacter;
struct FCustomGameplayEventData;

UCLASS(Blueprintable, BlueprintType)
class MANAGER_API UCustomAbility : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UCustomASC* OwnerASC;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	AActor* AvatarActor;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	ACharacter* OwnerCharacter;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cooldown")
	float CooldownDuration;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cost")
	float CostAmount;

protected:
	// ability 식별 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer AbilityTags;

	// 실행 불가 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer ActivationBlockedTags;

	// 조건 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer ActivationRequiredTags;

	// 실행 중 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer ActivationOwnedTags;

	// 실행 시, 다른 ability 강제 종료 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer CancelAbilitiesWithTag;

	// 실행 중, 다른 ability 실행 차단 태그
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FGameplayTagContainer BlockAbilitiesWithTag;

	// 쿨타임 도는 중 태그
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cooldown")
	FGameplayTagContainer CooldownTags;
protected:
	// (새로 추가!) 자식 클래스에서 이벤트 데이터를 받아서 로직을 짤 때 오버라이드할 함수
	virtual void ActivateAbilityWithEvent(const FCustomGameplayEventData& Payload);
public:
	UCustomAbility();

	void InitializeAbility(UCustomASC* InASC);
	virtual UWorld* GetWorld() const override;
	virtual bool TryActivateAbility();
	virtual bool TryActivateAbilityWithEvent(const FCustomGameplayEventData& Payload);

	virtual void CancelAbility();

	const FGameplayTagContainer& GetAbilityTags() const;
protected:
	virtual bool CanExecute() const;
	virtual void ActivateAbility();
	virtual void CommitAbility();
	virtual void EndAbility(bool bWasCancelled);

};
