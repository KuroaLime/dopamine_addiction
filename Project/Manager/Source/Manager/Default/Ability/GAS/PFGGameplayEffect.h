// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PFGGameplayEffect.generated.h"

class UPFGASC;

UENUM(BlueprintType)
enum class EPFGEffectAttribute : uint8
{
	Health,
};

UENUM(BlueprintType)
enum class EPFGEffectDurationType : uint8
{
	Instant,
	Duration,
	Infinite
};

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class MANAGER_API UPFGGameplayEffect : public UObject
{
	GENERATED_BODY()
	
public:
	// 클라이언트가 표시용 데이터(아이콘/이름 등)를 조회할 때 쓸 수 있는 정적 식별자.
	// 0은 "미지정".
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	int32 EffectID = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	EPFGEffectAttribute TargetAttribute = EPFGEffectAttribute::Health;

	// 한 번 적용(또는 1틱)당 변화량. 데미지는 음수.
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float Magnitude = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	EPFGEffectDurationType DurationType = EPFGEffectDurationType::Instant;

	UPROPERTY(EditDefaultsOnly, Category = "Effect", meta = (EditCondition = "DurationType != EPFGEffectDurationType::Instant"))
	float Duration = 0.f;

	// 0보다 크면 주기적으로 Magnitude를 반복 적용 (도트/HoT)
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float Period = 0.f;

	// 적용 시 대상에게 부여되는 태그 (버프/디버프 식별용)
	UPROPERTY(EditDefaultsOnly, Category = "Effect|Tags")
	FGameplayTagContainer GrantedTags;
};

USTRUCT()
struct FActivePFGGameplayEffect : public FFastArraySerializerItem
{
	GENERATED_BODY()

	// 표시용 식별자 (아이콘/이름 조회용)
	UPROPERTY()
	int32 EffectID = 0;

	// ----- Snapshot: 적용 시점의 Effect 데이터 복사본 -----
	UPROPERTY()
	EPFGEffectAttribute TargetAttribute = EPFGEffectAttribute::Health;

	UPROPERTY()
	float Magnitude = 0.f;

	UPROPERTY()
	float Period = 0.f;

	UPROPERTY()
	FGameplayTagContainer GrantedTags;
	// -----------------------------------------------------

	// 만료 예정 시각 (서버 GameState 기준 ServerWorldTimeSeconds). Infinite는 -1.
	UPROPERTY()
	float ExpirationTime = -1.f;

	// 이 Active Effect를 부여한 어빌리티/소스 식별 (선택, 디버프 출처 표시용)
	UPROPERTY()
	int32 SourceAbilityID = -1;

	UPROPERTY()
	int32 ActiveHandle = -1;
};

USTRUCT()
struct FActivePFGGameplayEffectsContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FActivePFGGameplayEffect> Items;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UPFGASC> OwnerASC;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FActivePFGGameplayEffect>(Items, DeltaParms, *this);
	}

	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FActivePFGGameplayEffectsContainer> : public TStructOpsTypeTraitsBase2<FActivePFGGameplayEffectsContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};