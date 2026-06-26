// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "PFGAbility.generated.h"

class UPFGASC;
class AActor;
class ACharacter;
struct FPFGGameplayEventData;

class IPhasePlayerStateInterface;
class IPhaseGameStateInterface;
class IAbilityOwnerInterface;
class IPhasePlayerControllerInterface;

UCLASS(Blueprintable, BlueprintType)
class MANAGER_API UPFGAbility : public UObject
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UPFGASC* OwnerASC;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	AActor* AvatarActor;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	ACharacter* OwnerCharacter;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cooldown")
	float CooldownDuration;

	// 0이면 코스트 없음(검사/차감 모두 스킵).
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cost")
	float CostAmount;

protected:
	// ability 식별 태그
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
	FGameplayTagContainer AbilityTags;

	// Trigger tag for HandleGameplayEvent (separate from AbilityTags)
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
	FGameplayTagContainer TriggerTags;

	// 실행 불가 태그: OwnerASC가 이 중 하나라도 가지고 있으면 CanExecute = false
	// (예: State.Stunned, State.Silenced)
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
	FGameplayTagContainer ActivationBlockedTags;

	// 조건 태그: OwnerASC가 이 태그를 "모두" 가지고 있어야 CanExecute = true
	// (예: State.Combat 중에만 사용 가능한 스킬)
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

	// 쿨다운 되는 중 태그
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Cooldown")
	FGameplayTagContainer CooldownTags;

	// 현재 이 어빌리티가 "실행 중"인지 여부.
	// TryActivateAbility(WithEvent) 성공 시 true, EndAbility에서 false로 복귀.
	// ActivationOwnedTags는 true인 동안만 OwnerASC->OwnedTags에 존재해야 하며,
	// EndAbility가 호출되지 않으면 영구적으로 남는다 -> 반드시 EndAbility 경로를 보장해야 함
	// (즉시 종료형 어빌리티는 ActivateAbility 마지막에 EndAbilityNow()를 호출해야 한다).
	UPROPERTY(VisibleAnywhere, Transient, Category = "GAS")
	bool bIsActive = false;

protected:
	virtual void ActivateAbilityWithEvent(const FPFGGameplayEventData& Payload);
public:
	UPFGAbility();

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	bool bLocalOnly = false;

	void InitializeAbility(UPFGASC* InASC);
	virtual UWorld* GetWorld() const override;
	virtual bool TryActivateAbility();
	virtual bool TryActivateAbilityWithEvent(const FPFGGameplayEventData& Payload);

	virtual void CancelAbility();
	virtual void LocalActivateWithOwner(AActor* InOwner) {}
	virtual void LocalCancelWithOwner(AActor* InOwner) {}

	// 현재 실행 중(ActivationOwnedTags가 부여된 상태)인지 여부.
	bool IsActive() const { return bIsActive; }

	// 정상 종료(취소 아님). 즉시 종료형 어빌리티는 ActivateAbility 마지막에 호출해
	// ActivationOwnedTags가 영구히 남지 않도록 해야 한다.
	// 지속형 어빌리티(채널링, 버프 지속 등)는 효과가 끝나는 시점(타이머/이벤트)에 호출한다.
	void EndAbilityNow();

	const FGameplayTagContainer& GetAbilityTags() const;
	const FGameplayTagContainer& GetTriggerTags() const;
	const FGameplayTagContainer& GetCooldownTags() const;
	const FGameplayTagContainer& GetActivationBlockedTags() const;
	const FGameplayTagContainer& GetActivationRequiredTags() const;
	const FGameplayTagContainer& GetBlockAbilitiesWithTag() const;
	const FGameplayTagContainer& GetCancelAbilitiesWithTag() const;
	float GetCooldownDuration() const;
	float GetCostAmount() const;

	virtual void ClearInterfaceCache();
protected:
	// CanExecute 검사 순서:
	// 1) OwnerASC 유효성
	// 2) bIsActive: 이미 실행 중이면 false (재진입 방지. 재진입을 허용하려는 어빌리티는
	//    CanExecute를 오버라이드해 이 검사를 건너뛸 수 있음)
	// 3) ActivationBlockedTags: OwnerASC가 하나라도 가지고 있으면 false
	// 4) ActivationRequiredTags: OwnerASC가 전부 가지고 있지 않으면 false
	// 5) CostAmount > 0 이면 OwnerASC->HasEnoughMana(CostAmount) 확인
	virtual bool CanExecute() const;
	virtual void ActivateAbility();

	// CommitAbility: CanExecute를 통과한 뒤 호출됨.
	// - CancelAbilitiesWithTag로 다른 어빌리티 취소
	// - CostAmount > 0 이면 OwnerASC->ApplyManaCost(CostAmount)로 마나 차감
	virtual void CommitAbility();
	virtual void EndAbility(bool bWasCancelled);

protected:
	IPhasePlayerStateInterface* GetPSInterface();
	IPhaseGameStateInterface* GetGSInterface();
	IAbilityOwnerInterface* GetOwnerInterface();
	IPhasePlayerControllerInterface* GetPCInterface();

private:
	IPhasePlayerStateInterface* CachedPSInterface = nullptr;
	IPhaseGameStateInterface* CachedGSInterface = nullptr;
	IAbilityOwnerInterface* CachedOwnerInterface = nullptr;
	IPhasePlayerControllerInterface* CachedPCInterface = nullptr;
};
