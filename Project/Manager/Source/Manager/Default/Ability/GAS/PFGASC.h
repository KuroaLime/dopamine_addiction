// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Default/Ability/GAS/PFGGameplayEffect.h"
#include "Default/Ability/GAS/PFGGameplayTagStack.h"
#include "PFGASC.generated.h"

class UPFGAbility;
class UPFGAttributeSet;
class UPFGGameplayEffect;
class AActor;
class UPFGASC;

USTRUCT(BlueprintType)
struct FPFGGameplayEventData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	AActor* Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite)
	AActor* TargetObject = nullptr;
};

/////////////////////////////////////////////////////////////////////////////
// PredictionKey
USTRUCT(BlueprintType)
struct FPFGPredictionKey
{
	GENERATED_BODY()

	UPROPERTY()
	int32 KeyValue = 0;

	bool IsValidKey() const { return KeyValue != 0; }

	bool operator==(const FPFGPredictionKey& Other) const { return KeyValue == Other.KeyValue; }
};

/////////////////////////////////////////////////////////////////////////////
// Ability Spec
USTRUCT()
struct FPFGAbilitySpec : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FPFGAbilitySpec() {}

	UPROPERTY()
	TSubclassOf<UPFGAbility> AbilityClass = nullptr;

	UPROPERTY()
	FGameplayTagContainer AbilityTags;

	UPROPERTY()
	FGameplayTagContainer TriggerTags;

	UPROPERTY()
	FGameplayTagContainer CooldownTags;

	UPROPERTY()
	float CooldownDuration = 0.f;

	// 쿨다운 종료 시각 (ServerWorldTimeSeconds 기준). 0이면 쿨다운 없음/만료됨.
	UPROPERTY()
	float CooldownEndTime = 0.f;

	UPROPERTY()
	int32 AbilityID = -1;

	UPROPERTY()
	int32 Level = 1;

	// ----- 서버 전용, 복제되지 않음 -----
	UPROPERTY(NotReplicated, Transient)
	UPFGAbility* AbilityInstance = nullptr;
};

USTRUCT()
struct FPFGAbilitySpecContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPFGAbilitySpec> Items;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UPFGASC> OwnerASC;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPFGAbilitySpec>(Items, DeltaParms, *this);
	}

	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FPFGAbilitySpecContainer> : public TStructOpsTypeTraitsBase2<FPFGAbilitySpecContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

UENUM(BlueprintType)
enum class EPFGAbilityActivationResult : uint8
{
	Failed_NoAbility,
	Failed_CannotExecute,
	Failed_OnCooldown,
	Failed_TagsBlocked,
	Failed_CostNotMet,
	LocalSuccess,
	RequestSentToServer
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UPFGASC : public UActorComponent
{
	GENERATED_BODY()

public:
	UPFGASC();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	/////////////////////////////////////////////////////////////////////
	// Ability 부여 / 제거 / 조회

	void GiveAbility(TSubclassOf<UPFGAbility> AbilityClass, int32 Level = 1, bool bAllowStacking = false);

	bool RemoveAbility(int32 AbilityID);
	bool RemoveAbilityByClass(TSubclassOf<UPFGAbility> AbilityClass);

	FPFGAbilitySpec* FindAbilitySpecByID(int32 AbilityID);
	FPFGAbilitySpec* FindAbilitySpecByClass(TSubclassOf<UPFGAbility> AbilityClass);
	FPFGAbilitySpec* FindAbilitySpecByTag(FGameplayTag Tag);

	// 클라이언트도 호출 가능: 서버에서만 존재하는 AbilityInstance 대신 CDO(클래스 디폴트 오브젝트)를
	// 통해 어빌리티 정의 정보(AbilityTags, CooldownTags 등 EditDefaultsOnly 값)를 조회한다.
	// UI(아이콘, 설명, 코스트 표시 등)는 이 함수를 사용해야 한다.
	static const UPFGAbility* GetAbilityCDO(TSubclassOf<UPFGAbility> AbilityClass);

	/////////////////////////////////////////////////////////////////////
	// Activation

	EPFGAbilityActivationResult TryActivateAbilityByTag(FGameplayTag AbilityTag);
	void CancelAbilitiesWithTag(const FGameplayTagContainer& TagsToCancel);

	void AddGameplayTags(const FGameplayTagContainer& TagsToAdd);
	void RemoveGameplayTags(const FGameplayTagContainer& TagsToRemove);
	bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const;
	bool HasMatchingGameplayTag(FGameplayTag Tag) const;

	// EventTag와 일치하는 TriggerTags를 가진 "모든" 어빌리티를 시도한다.
	// (쿨다운/CanExecute 등으로 개별 실패해도 나머지는 계속 시도됨)
	// 반환값: 하나 이상 활성화에 성공했는지 여부.
	bool HandleGameplayEvent(FGameplayTag EventTag, const FPFGGameplayEventData& Payload);

	/////////////////////////////////////////////////////////////////////
	// Cooldown - CooldownEndTime 기반 (lazy cleanup)

	bool IsAbilityOnCooldown(FPFGAbilitySpec& Spec);
	void StartCooldown(FPFGAbilitySpec& Spec);

	float GetServerWorldTime() const;

public:
	/////////////////////////////////////////////////////////////////////
	// AttributeSet / GameplayEffect

	UPROPERTY(Replicated, VisibleAnywhere, Category = "GAS|Attributes")
	UPFGAttributeSet* AttributeSet = nullptr;

	void ApplyGameplayEffectToSelf(UPFGGameplayEffect* Effect, int32 SourceAbilityID = -1);

	bool RemoveActiveEffect(int32 ActiveHandle);

	// Infinite Effect 등 명시적 제거가 필요한 효과를 위한 추가 API.
	// - ByEffectID: 같은 EffectID를 가진 ActiveEffect를 모두 제거 (예: Dispel로 "Poison" 전부 제거)
	// - BySourceAbilityID: 특정 어빌리티가 부여한 ActiveEffect를 모두 제거
	//   (예: 채널링 스킬 종료 시 그 스킬이 건 실드/버프 제거)
	// 반환값: 제거된 개수.
	int32 RemoveActiveEffectsByEffectID(int32 EffectID);
	int32 RemoveActiveEffectsBySourceAbility(int32 SourceAbilityID);

	// 현재 활성화된 Infinite Effect 목록 조회 (UI/디버그용)
	void GetActiveInfiniteEffects(TArray<FActivePFGGameplayEffect>& OutEffects) const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_AbilitySpecs, VisibleAnywhere, Category = "GAS")
	FPFGAbilitySpecContainer AbilitySpecContainer;

	UFUNCTION()
	void OnRep_AbilitySpecs();

	/////////////////////////////////////////////////////////////////////
	// GameplayTag Stack
	//
	// OwnedTags(FGameplayTagContainer, Set 방식)를 TagStacks(Count 방식)로 교체.
	// - HasAnyMatchingGameplayTags / HasMatchingGameplayTag는 TagStacks.HasAny/HasTag로 동작.
	// - AddGameplayTags/RemoveGameplayTags는 각 태그의 카운트를 +1/-1.
	// - 동일 태그가 여러 출처(Effect 2개 등)에서 부여되어도 한쪽 제거 시 다른 쪽이 유지됨.
	UPROPERTY(ReplicatedUsing = OnRep_TagStacks, VisibleAnywhere, Category = "GAS")
	FPFGGameplayTagStackContainer TagStacks;

	UFUNCTION()
	void OnRep_TagStacks();

	// 클라이언트: 이전 TagCountMap 스냅샷 (변경분 델리게이트 계산용)
	TMap<FGameplayTag, int32> PreviousTagCountMap;

	void BroadcastTagStackDelta();

	UPROPERTY(ReplicatedUsing = OnRep_ActiveEffects, VisibleAnywhere, Category = "GAS")
	FActivePFGGameplayEffectsContainer ActiveEffectsContainer;

	UFUNCTION()
	void OnRep_ActiveEffects();

	int32 NextActiveEffectHandle = 0;

	// (문제 5) ActiveHandle -> Items 인덱스 O(1) 조회 캐시.
	// Items에서 항목이 추가/제거될 때마다 함께 갱신한다.
	TMap<int32, int32> ActiveEffectHandleToIndex;

	void RebuildActiveEffectIndexMap();

	UPROPERTY(Transient)
	TMap<int32, FTimerHandle> EffectExpirationTimerHandles;

	UPROPERTY(Transient)
	TMap<int32, FTimerHandle> EffectPeriodicTimerHandles;

	FActivePFGGameplayEffect* FindActiveEffect(int32 ActiveHandle);

	void OnEffectExpired(int32 ActiveHandle);
	void OnEffectPeriodic(int32 ActiveHandle);

	void ApplySnapshotToAttributeSet(const FActivePFGGameplayEffect& Active);

protected:
	bool InternalTryActivateAbilityByTag(FGameplayTag AbilityTag, FPFGPredictionKey PredictionKey);

	int32 NextAbilityID = 0;

public:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_TryActivateAbilityByTag(FGameplayTag AbilityTag, FPFGPredictionKey PredictionKey);

	UFUNCTION(Client, Reliable)
	void ClientRPC_AbilityActivationConfirmed(FPFGPredictionKey PredictionKey, bool bSuccess);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_CancelAbilitiesWithTag(FGameplayTagContainer TagsToCancel);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_SendGameplayEvent(FGameplayTag EventTag, const FPFGGameplayEventData& Payload);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_BroadcastGameplayEvent(FGameplayTag EventTag, const FPFGGameplayEventData& Payload);

	UFUNCTION(Client, Reliable)
	void ClientRPC_ReceiveGameplayEvent(FGameplayTag EventTag, const FPFGGameplayEventData& Payload);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayEventReceived, FGameplayTag, const FPFGGameplayEventData&);
	FOnGameplayEventReceived OnGameplayEventReceived;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbilityActivationConfirmed, FPFGPredictionKey, bool /*bSuccess*/);
	FOnAbilityActivationConfirmed OnAbilityActivationConfirmed;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOwnedTagsChanged, const FGameplayTagContainer& /*AddedTags*/, const FGameplayTagContainer& /*RemovedTags*/);
	FOnOwnedTagsChanged OnOwnedTagsChanged;

protected:
	int32 NextLocalPredictionKey = 1;

public:
	FPFGPredictionKey GenerateNewPredictionKey();

};
