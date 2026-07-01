// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/GAS/PFGGameplayTagStack.h"

bool FPFGGameplayTagStackContainer::ApplyTagCountDelta(FGameplayTag Tag, int32 Delta)
{
	if (Delta == 0 || !Tag.IsValid())
	{
		return false;
	}

	const int32 OldCount = GetTagCount(Tag);
	const int32 NewCount = FMath::Max(0, OldCount + Delta);

	if (OldCount == NewCount)
	{
		return false;
	}

	// Items 배열에서 해당 Tag 항목 찾기
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].Tag == Tag)
		{
			FoundIndex = i;
			break;
		}
	}

	if (NewCount > 0)
	{
		if (FoundIndex != INDEX_NONE)
		{
			Items[FoundIndex].StackCount = NewCount;
			MarkItemDirty(Items[FoundIndex]);
		}
		else
		{
			FPFGGameplayTagStack NewStack(Tag, NewCount);
			Items.Add(NewStack);
			MarkItemDirty(Items.Last());
		}
	}
	else // NewCount == 0 -> 항목 제거
	{
		if (FoundIndex != INDEX_NONE)
		{
			Items.RemoveAt(FoundIndex);
			MarkArrayDirty();
		}
	}

	TagCountMap.Add(Tag, NewCount);
	if (NewCount == 0)
	{
		TagCountMap.Remove(Tag);
	}

	// 존재 여부(0 <-> 양수) 전환이 있었는지
	const bool bWasPresent = OldCount > 0;
	const bool bIsPresent = NewCount > 0;
	return bWasPresent != bIsPresent;
}

void FPFGGameplayTagStackContainer::RebuildTagCountMap()
{
	TagCountMap.Reset();
	for (const FPFGGameplayTagStack& Stack : Items)
	{
		if (Stack.StackCount > 0)
		{
			TagCountMap.Add(Stack.Tag, Stack.StackCount);
		}
	}
}

void FPFGGameplayTagStackContainer::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	// 제거된 인덱스들을 미리 맵에서 빼둔다 (Items가 아직 그대로인 시점이라
	// 인덱스 기반으로 직접 제거 가능)
	for (int32 Index : RemovedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			TagCountMap.Remove(Items[Index].Tag);
		}
	}
}

void FPFGGameplayTagStackContainer::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	RebuildTagCountMap();
}

void FPFGGameplayTagStackContainer::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	RebuildTagCountMap();
}
