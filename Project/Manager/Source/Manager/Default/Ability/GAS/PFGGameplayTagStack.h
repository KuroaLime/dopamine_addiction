// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PFGGameplayTagStack.generated.h"

class UPFGASC;

/////////////////////////////////////////////////////////////////////////////
// GameplayTag Stack
//
// 실제 GAS의 FGameplayTagCountContainer와 동일한 의도:
// - 동일 태그가 여러 출처(Effect, Ability 등)에서 중복 부여될 수 있다.
// - 단순 FGameplayTagContainer(Set)는 "있다/없다"만 표현 가능해서,
//   한 출처가 RemoveTags를 호출하면 다른 출처가 부여한 동일 태그까지 사라진다.
// - StackCount를 두어 Add/Remove를 +1/-1로 처리하고, 0이 될 때만 실제로 제거한다.
USTRUCT()
struct FPFGGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FPFGGameplayTagStack() {}
	FPFGGameplayTagStack(FGameplayTag InTag, int32 InCount) : Tag(InTag), StackCount(InCount) {}

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

USTRUCT()
struct FPFGGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPFGGameplayTagStack> Items;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UPFGASC> OwnerASC;

	// 빠른 조회용 캐시 (서버: 항상 최신, 클라이언트: FastArray 콜백에서 갱신)
	UPROPERTY(NotReplicated)
	TMap<FGameplayTag, int32> TagCountMap;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPFGGameplayTagStack>(Items, DeltaParms, *this);
	}

	// 서버 전용: Delta 적용. Count<=0이 되면 Items에서 제거.
	// 반환값: 이 호출로 인해 "0->양수" 또는 "양수->0" 전환이 발생했는지 (= 태그 존재 여부가 바뀌었는지)
	bool ApplyTagCountDelta(FGameplayTag Tag, int32 Delta);

	int32 GetTagCount(FGameplayTag Tag) const
	{
		const int32* Count = TagCountMap.Find(Tag);
		return Count ? *Count : 0;
	}

	bool HasTag(FGameplayTag Tag) const
	{
		return GetTagCount(Tag) > 0;
	}

	bool HasAny(const FGameplayTagContainer& Tags) const
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (HasTag(Tag))
			{
				return true;
			}
		}
		return false;
	}

	// 클라이언트: FastArray 콜백에서 TagCountMap을 Items와 동기화
	void RebuildTagCountMap();

	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FPFGGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FPFGGameplayTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
