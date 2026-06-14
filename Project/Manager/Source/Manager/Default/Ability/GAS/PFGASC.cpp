// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Default/Ability/GAS/PFGAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UPFGASC::UPFGASC()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UPFGASC::BeginPlay()
{
	Super::BeginPlay();

	AbilitySpecContainer.OwnerASC = this;
	ActiveEffectsContainer.OwnerASC = this;
	TagStacks.OwnerASC = this;

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority() && !AttributeSet)
	{
		AttributeSet = NewObject<UPFGAttributeSet>(this, UPFGAttributeSet::StaticClass());
	}
}

void UPFGASC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();

		for (auto& Pair : EffectExpirationTimerHandles)
		{
			TM.ClearTimer(Pair.Value);
		}
		for (auto& Pair : EffectPeriodicTimerHandles)
		{
			TM.ClearTimer(Pair.Value);
		}
	}

	EffectExpirationTimerHandles.Empty();
	EffectPeriodicTimerHandles.Empty();

	Super::EndPlay(EndPlayReason);
}


// Called every frame
void UPFGASC::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 복제할 변수들 등록
void UPFGASC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPFGASC, TagStacks);
	DOREPLIFETIME(UPFGASC, AbilitySpecContainer);
	DOREPLIFETIME(UPFGASC, ActiveEffectsContainer);
	DOREPLIFETIME(UPFGASC, AttributeSet);
}

bool UPFGASC::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	if (AttributeSet)
	{
		bWroteSomething |= Channel->ReplicateSubobject(AttributeSet, *Bunch, *RepFlags);
	}

	return bWroteSomething;
}

float UPFGASC::GetServerWorldTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;

	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

/////////////////////////////////////////////////////////////////////////////
// (문제 1) Ability CDO 조회 - 클라이언트도 정의 데이터 접근 가능
/////////////////////////////////////////////////////////////////////////////

const UPFGAbility* UPFGASC::GetAbilityCDO(TSubclassOf<UPFGAbility> AbilityClass)
{
	if (!AbilityClass) return nullptr;
	return GetDefault<UPFGAbility>(AbilityClass);
}

/////////////////////////////////////////////////////////////////////////////
// (문제 2) GameplayTag Stack
/////////////////////////////////////////////////////////////////////////////

void UPFGASC::OnRep_TagStacks()
{
	// RebuildTagCountMap은 FastArray Pre/PostReplicated 콜백에서 이미 호출됨.
	// 여기서는 변경분(Added/Removed) 델리게이트만 발행.

	FString DebugMsg = FString::Printf(TEXT("OnRep_TagStacks(rebuilt): TagCount=%d"), TagStacks.TagCountMap.Num());
	for (const auto& Pair : TagStacks.TagCountMap)
	{
		DebugMsg += FString::Printf(TEXT(" [%s=%d]"), *Pair.Key.ToString(), Pair.Value);
	}
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, DebugMsg);

	BroadcastTagStackDelta();
}

void UPFGASC::BroadcastTagStackDelta()
{
	FGameplayTagContainer AddedTags;
	FGameplayTagContainer RemovedTags;

	for (const auto& Pair : TagStacks.TagCountMap)
	{
		if (!PreviousTagCountMap.Contains(Pair.Key))
		{
			AddedTags.AddTag(Pair.Key);
		}
	}
	for (const auto& Pair : PreviousTagCountMap)
	{
		if (!TagStacks.TagCountMap.Contains(Pair.Key))
		{
			RemovedTags.AddTag(Pair.Key);
		}
	}

	if (!AddedTags.IsEmpty() || !RemovedTags.IsEmpty())
	{
		OnOwnedTagsChanged.Broadcast(AddedTags, RemovedTags);
	}

	PreviousTagCountMap = TagStacks.TagCountMap;
}

void UPFGASC::AddGameplayTags(const FGameplayTagContainer& TagsToAdd)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return;

	if (TagsToAdd.IsEmpty()) return;

	bool bAnyPresenceChanged = false;
	for (const FGameplayTag& Tag : TagsToAdd)
	{
		if (TagStacks.ApplyTagCountDelta(Tag, +1))
		{
			bAnyPresenceChanged = true;
		}
	}

	if (bAnyPresenceChanged)
	{
		BroadcastTagStackDelta();
	}
}

void UPFGASC::RemoveGameplayTags(const FGameplayTagContainer& TagsToRemove)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return;

	if (TagsToRemove.IsEmpty()) return;

	bool bAnyPresenceChanged = false;
	for (const FGameplayTag& Tag : TagsToRemove)
	{
		if (TagStacks.ApplyTagCountDelta(Tag, -1))
		{
			bAnyPresenceChanged = true;
		}
	}

	if (bAnyPresenceChanged)
	{
		BroadcastTagStackDelta();
	}
}

bool UPFGASC::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return TagStacks.HasAny(TagContainer);
}

bool UPFGASC::HasMatchingGameplayTag(FGameplayTag Tag) const
{
	return TagStacks.HasTag(Tag);
}

/////////////////////////////////////////////////////////////////////////////
// FastArray 콜백
/////////////////////////////////////////////////////////////////////////////

void FPFGAbilitySpecContainer::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize) {}
void FPFGAbilitySpecContainer::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize) {}
void FPFGAbilitySpecContainer::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize) {}

void UPFGASC::OnRep_AbilitySpecs() {}

void FActivePFGGameplayEffectsContainer::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize) {}
void FActivePFGGameplayEffectsContainer::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize) {}
void FActivePFGGameplayEffectsContainer::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize) {}

void UPFGASC::OnRep_ActiveEffects()
{
	RebuildActiveEffectIndexMap();
}

void UPFGASC::RebuildActiveEffectIndexMap()
{
	ActiveEffectHandleToIndex.Reset();
	for (int32 i = 0; i < ActiveEffectsContainer.Items.Num(); ++i)
	{
		ActiveEffectHandleToIndex.Add(ActiveEffectsContainer.Items[i].ActiveHandle, i);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Ability 부여 / 제거 / 조회
/////////////////////////////////////////////////////////////////////////////

void UPFGASC::GiveAbility(TSubclassOf<UPFGAbility> AbilityClass, int32 Level, bool bAllowStacking)
{
	if (!AbilityClass) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

	if (!bAllowStacking)
	{
		if (FPFGAbilitySpec* Existing = FindAbilitySpecByClass(AbilityClass))
		{
			if (Level > Existing->Level)
			{
				Existing->Level = Level;
				AbilitySpecContainer.MarkItemDirty(*Existing);
			}
			return;
		}
	}

	UPFGAbility* NewAbility = NewObject<UPFGAbility>(this, AbilityClass);
	if (!NewAbility) return;

	NewAbility->InitializeAbility(this);

	FPFGAbilitySpec NewSpec;
	NewSpec.AbilityClass = AbilityClass;
	NewSpec.AbilityTags = NewAbility->GetAbilityTags();
	NewSpec.TriggerTags = NewAbility->GetTriggerTags();
	NewSpec.CooldownTags = NewAbility->GetCooldownTags();
	NewSpec.CooldownDuration = NewAbility->GetCooldownDuration();
	NewSpec.AbilityID = NextAbilityID++;
	NewSpec.Level = Level;
	NewSpec.AbilityInstance = NewAbility;

	AbilitySpecContainer.Items.Add(NewSpec);
	AbilitySpecContainer.MarkItemDirty(AbilitySpecContainer.Items.Last());
}

bool UPFGASC::RemoveAbility(int32 AbilityID)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return false;

	for (int32 i = 0; i < AbilitySpecContainer.Items.Num(); ++i)
	{
		FPFGAbilitySpec& Spec = AbilitySpecContainer.Items[i];
		if (Spec.AbilityID == AbilityID)
		{
			if (Spec.AbilityInstance)
			{
				// (문제 7) 진행 중인 어빌리티를 먼저 취소(타이머/델리게이트 정리)한 뒤,
				// GC가 수거하도록 명시적으로 Mark. 참조를 끊기 전에 CancelAbility를 호출해
				// EndAbility 내부에서 OwnerASC 등 멤버를 안전하게 사용할 수 있게 한다.
				Spec.AbilityInstance->CancelAbility();
				Spec.AbilityInstance->ClearInterfaceCache();
				Spec.AbilityInstance->MarkAsGarbage();
				Spec.AbilityInstance = nullptr;
			}

			if (!Spec.CooldownTags.IsEmpty())
			{
				RemoveGameplayTags(Spec.CooldownTags);
			}

			AbilitySpecContainer.Items.RemoveAt(i);
			AbilitySpecContainer.MarkArrayDirty();
			return true;
		}
	}
	return false;
}

bool UPFGASC::RemoveAbilityByClass(TSubclassOf<UPFGAbility> AbilityClass)
{
	if (FPFGAbilitySpec* Spec = FindAbilitySpecByClass(AbilityClass))
	{
		return RemoveAbility(Spec->AbilityID);
	}
	return false;
}

FPFGAbilitySpec* UPFGASC::FindAbilitySpecByID(int32 AbilityID)
{
	for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
	{
		if (Spec.AbilityID == AbilityID)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FPFGAbilitySpec* UPFGASC::FindAbilitySpecByClass(TSubclassOf<UPFGAbility> AbilityClass)
{
	for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
	{
		if (Spec.AbilityClass == AbilityClass)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FPFGAbilitySpec* UPFGASC::FindAbilitySpecByTag(FGameplayTag Tag)
{
	for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
	{
		if (Spec.AbilityTags.HasTag(Tag))
		{
			return &Spec;
		}
	}
	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
// Activation
/////////////////////////////////////////////////////////////////////////////

EPFGAbilityActivationResult UPFGASC::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return EPFGAbilityActivationResult::Failed_NoAbility;

	FPFGAbilitySpec* Spec = FindAbilitySpecByTag(AbilityTag);
	if (!Spec)
	{
		return EPFGAbilityActivationResult::Failed_NoAbility;
	}

	if (IsAbilityOnCooldown(*Spec))
	{
		return EPFGAbilityActivationResult::Failed_OnCooldown;
	}

	// 클라이언트에서도 CDO 기반으로 사전 검사(빠른 실패) 가능.
	// 실제 권한 있는 판정은 서버의 InternalTryActivateAbilityByTag(CanExecute)에서 수행됨.
	if (const UPFGAbility* CDO = GetAbilityCDO(Spec->AbilityClass))
	{
		if (!CDO->GetActivationBlockedTags().IsEmpty() &&
			HasAnyMatchingGameplayTags(CDO->GetActivationBlockedTags()))
		{
			return EPFGAbilityActivationResult::Failed_TagsBlocked;
		}

		for (const FGameplayTag& RequiredTag : CDO->GetActivationRequiredTags())
		{
			if (!HasMatchingGameplayTag(RequiredTag))
			{
				return EPFGAbilityActivationResult::Failed_TagsBlocked;
			}
		}
	}

	if (OwnerActor->HasAuthority())
	{
		const bool bSuccess = InternalTryActivateAbilityByTag(AbilityTag, FPFGPredictionKey());
		return bSuccess
			? EPFGAbilityActivationResult::LocalSuccess
			: EPFGAbilityActivationResult::Failed_CannotExecute;
	}
	else
	{
		const FPFGPredictionKey NewKey = GenerateNewPredictionKey();

		// TODO(Prediction 확장): 로컬 cosmetic 즉시 재생 + NewKey 보류 목록 저장

		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			if (UPFGAbility* CDO = Spec->AbilityClass ? Spec->AbilityClass->GetDefaultObject<UPFGAbility>() : nullptr)
			{
				CDO->LocalActivateWithOwner(GetOwner());

				if (CDO->bLocalOnly)
				{
					return EPFGAbilityActivationResult::LocalSuccess;
				}
			}
		}

		ServerRPC_TryActivateAbilityByTag(AbilityTag, NewKey);
		return EPFGAbilityActivationResult::RequestSentToServer;
	}
}

void UPFGASC::CancelAbilitiesWithTag(const FGameplayTagContainer& TagsToCancel)
{
	if (TagsToCancel.IsEmpty()) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	if (OwnerActor->HasAuthority())
	{
		for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
		{
			if (Spec.AbilityInstance && Spec.AbilityTags.HasAny(TagsToCancel))
			{
				Spec.AbilityInstance->CancelAbility();
			}
		}
	}
	else
	{
		APawn* OwnerPawn = Cast<APawn>(OwnerActor);
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
			{
				if (Spec.AbilityTags.HasAny(TagsToCancel))
				{
					if (UPFGAbility* CDO = Spec.AbilityClass
						? Spec.AbilityClass->GetDefaultObject<UPFGAbility>()
						: nullptr)
					{
						CDO->LocalCancelWithOwner(OwnerActor);
					}
				}
			}
		}

		ServerRPC_CancelAbilitiesWithTag(TagsToCancel);
	}
}

bool UPFGASC::InternalTryActivateAbilityByTag(FGameplayTag AbilityTag, FPFGPredictionKey PredictionKey)
{
	FPFGAbilitySpec* Spec = FindAbilitySpecByTag(AbilityTag);
	if (!Spec || !Spec->AbilityInstance)
	{
		return false;
	}

	if (IsAbilityOnCooldown(*Spec))
	{
		return false;
	}

	// 최종 권한 판정은 TryActivateAbility 내부의 CanExecute (BlockedTags/RequiredTags/Cost 포함)
	const bool bActivated = Spec->AbilityInstance->TryActivateAbility();
	if (bActivated && Spec->CooldownDuration > 0.f && !Spec->CooldownTags.IsEmpty())
	{
		StartCooldown(*Spec);
	}

	return bActivated;
}

void UPFGASC::ServerRPC_TryActivateAbilityByTag_Implementation(FGameplayTag AbilityTag, FPFGPredictionKey PredictionKey)
{
	const bool bSuccess = InternalTryActivateAbilityByTag(AbilityTag, PredictionKey);

	if (PredictionKey.IsValidKey())
	{
		ClientRPC_AbilityActivationConfirmed(PredictionKey, bSuccess);
	}
}

bool UPFGASC::ServerRPC_TryActivateAbilityByTag_Validate(FGameplayTag AbilityTag, FPFGPredictionKey PredictionKey)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	return FindAbilitySpecByTag(AbilityTag) != nullptr;
}

void UPFGASC::ClientRPC_AbilityActivationConfirmed_Implementation(FPFGPredictionKey PredictionKey, bool bSuccess)
{
	OnAbilityActivationConfirmed.Broadcast(PredictionKey, bSuccess);
}

void UPFGASC::ServerRPC_CancelAbilitiesWithTag_Implementation(FGameplayTagContainer TagsToCancel)
{
	CancelAbilitiesWithTag(TagsToCancel);
}

bool UPFGASC::ServerRPC_CancelAbilitiesWithTag_Validate(FGameplayTagContainer TagsToCancel)
{
	return !TagsToCancel.IsEmpty();
}

FPFGPredictionKey UPFGASC::GenerateNewPredictionKey()
{
	FPFGPredictionKey Key;
	Key.KeyValue = NextLocalPredictionKey++;
	return Key;
}

/////////////////////////////////////////////////////////////////////////////
// HandleGameplayEvent
/////////////////////////////////////////////////////////////////////////////

bool UPFGASC::HandleGameplayEvent(FGameplayTag EventTag, const FPFGGameplayEventData& Payload)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return false;

	bool bAnyActivated = false;

	// EventTag와 일치하는 TriggerTags를 가진 모든 어빌리티를 시도한다.
	// 개별 어빌리티가 쿨다운/CanExecute로 실패해도 나머지 어빌리티는 계속 시도된다.
	//
	// 주의: TryActivateAbilityWithEvent 내부(CancelAbilitiesWithTag 등)에서
	// AbilitySpecContainer.Items가 변경(Remove 등)될 가능성이 있는 코드베이스라면
	// 인덱스 기반 순회로 바꾸는 것을 고려할 것. 현재 구현에서는 어빌리티 실행 경로가
	// Spec 배열을 직접 변형하지 않으므로 range-for로 충분하다.
	for (FPFGAbilitySpec& Spec : AbilitySpecContainer.Items)
	{
		if (Spec.AbilityInstance && Spec.TriggerTags.HasTag(EventTag))
		{
			if (IsAbilityOnCooldown(Spec))
			{
				continue;
			}

			const bool bActivated = Spec.AbilityInstance->TryActivateAbilityWithEvent(Payload);
			if (bActivated)
			{
				bAnyActivated = true;

				if (Spec.CooldownDuration > 0.f && !Spec.CooldownTags.IsEmpty())
				{
					StartCooldown(Spec);
				}
			}
		}
	}

	return bAnyActivated;
}

void UPFGASC::ServerRPC_SendGameplayEvent_Implementation(FGameplayTag EventTag, const FPFGGameplayEventData& Payload)
{
	const bool bHandled = HandleGameplayEvent(EventTag, Payload);

	if (bHandled)
	{
		MulticastRPC_BroadcastGameplayEvent(EventTag, Payload);
	}
}

bool UPFGASC::ServerRPC_SendGameplayEvent_Validate(FGameplayTag EventTag, const FPFGGameplayEventData& Payload)
{
	if (!EventTag.IsValid())
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (Payload.Instigator && Payload.Instigator != OwnerActor)
	{
		return false;
	}

	if (!OwnerActor->HasLocalNetOwner() && OwnerActor->GetNetConnection() == nullptr)
	{
		return false;
	}

	if (Payload.TargetObject && !IsValid(Payload.TargetObject))
	{
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Multicast / Client RPC
/////////////////////////////////////////////////////////////////////////////

void UPFGASC::MulticastRPC_BroadcastGameplayEvent_Implementation(FGameplayTag EventTag, const FPFGGameplayEventData& Payload)
{
	OnGameplayEventReceived.Broadcast(EventTag, Payload);
}

void UPFGASC::ClientRPC_ReceiveGameplayEvent_Implementation(FGameplayTag EventTag, const FPFGGameplayEventData& Payload)
{
	OnGameplayEventReceived.Broadcast(EventTag, Payload);
}

/////////////////////////////////////////////////////////////////////////////
// Cooldown - CooldownEndTime 기반 (lazy cleanup)
/////////////////////////////////////////////////////////////////////////////

bool UPFGASC::IsAbilityOnCooldown(FPFGAbilitySpec& Spec)
{
	if (Spec.CooldownTags.IsEmpty() || Spec.CooldownEndTime <= 0.f)
	{
		return false;
	}

	const float Now = GetServerWorldTime();

	if (Spec.CooldownEndTime > Now)
	{
		return true;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		RemoveGameplayTags(Spec.CooldownTags);
		Spec.CooldownEndTime = 0.f;
		AbilitySpecContainer.MarkItemDirty(Spec);
	}

	return false;
}

void UPFGASC::StartCooldown(FPFGAbilitySpec& Spec)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return;

	if (Spec.CooldownTags.IsEmpty() || Spec.CooldownDuration <= 0.f)
	{
		return;
	}

	AddGameplayTags(Spec.CooldownTags);

	Spec.CooldownEndTime = GetServerWorldTime() + Spec.CooldownDuration;
	AbilitySpecContainer.MarkItemDirty(Spec);
}

/////////////////////////////////////////////////////////////////////////////
// GameplayEffect / AttributeSet - Snapshot 기반, 이벤트 드리븐
/////////////////////////////////////////////////////////////////////////////

FActivePFGGameplayEffect* UPFGASC::FindActiveEffect(int32 ActiveHandle)
{
	// (문제 5) O(1) 조회
	if (const int32* Index = ActiveEffectHandleToIndex.Find(ActiveHandle))
	{
		if (ActiveEffectsContainer.Items.IsValidIndex(*Index) &&
			ActiveEffectsContainer.Items[*Index].ActiveHandle == ActiveHandle)
		{
			return &ActiveEffectsContainer.Items[*Index];
		}
	}

	// 캐시 미스(인덱스 불일치) 시 1회 재구축 후 재시도 - 방어적 처리
	RebuildActiveEffectIndexMap();
	if (const int32* Index = ActiveEffectHandleToIndex.Find(ActiveHandle))
	{
		if (ActiveEffectsContainer.Items.IsValidIndex(*Index))
		{
			return &ActiveEffectsContainer.Items[*Index];
		}
	}
	return nullptr;
}

void UPFGASC::ApplySnapshotToAttributeSet(const FActivePFGGameplayEffect& Active)
{
	if (!AttributeSet) return;

	switch (Active.TargetAttribute)
	{
	case EPFGEffectAttribute::Health:
		AttributeSet->ApplyHealthDelta(Active.Magnitude);
		break;
	}
}

void UPFGASC::ApplyGameplayEffectToSelf(UPFGGameplayEffect* Effect, int32 SourceAbilityID)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Effect || !AttributeSet) return;

	if (!Effect->GrantedTags.IsEmpty())
	{
		AddGameplayTags(Effect->GrantedTags);
	}

	const float Now = GetServerWorldTime();

	switch (Effect->DurationType)
	{
	case EPFGEffectDurationType::Instant:
	{
		switch (Effect->TargetAttribute)
		{
		case EPFGEffectAttribute::Health:
			AttributeSet->ApplyHealthDelta(Effect->Magnitude);
			break;
		}
		break;
	}
	case EPFGEffectDurationType::Duration:
	case EPFGEffectDurationType::Infinite:
	{
		const int32 NewHandle = NextActiveEffectHandle++;

		FActivePFGGameplayEffect Active;
		Active.EffectID = Effect->EffectID;
		Active.ActiveHandle = NewHandle;
		Active.SourceAbilityID = SourceAbilityID;

		Active.TargetAttribute = Effect->TargetAttribute;
		Active.Magnitude = Effect->Magnitude;
		Active.Period = Effect->Period;
		Active.GrantedTags = Effect->GrantedTags;

		Active.ExpirationTime = (Effect->DurationType == EPFGEffectDurationType::Infinite)
			? -1.f
			: (Now + Effect->Duration);

		ActiveEffectsContainer.Items.Add(Active);
		ActiveEffectsContainer.MarkItemDirty(ActiveEffectsContainer.Items.Last());

		// (문제 5) 인덱스 맵 갱신
		ActiveEffectHandleToIndex.Add(NewHandle, ActiveEffectsContainer.Items.Num() - 1);

		UWorld* World = GetWorld();

		if (Effect->DurationType == EPFGEffectDurationType::Duration && World)
		{
			FTimerHandle ExpireHandle;
			World->GetTimerManager().SetTimer(
				ExpireHandle,
				FTimerDelegate::CreateUObject(this, &UPFGASC::OnEffectExpired, NewHandle),
				Effect->Duration,
				false
			);
			EffectExpirationTimerHandles.Add(NewHandle, ExpireHandle);
		}

		if (Effect->Period > 0.f && World)
		{
			FTimerHandle PeriodicHandle;
			World->GetTimerManager().SetTimer(
				PeriodicHandle,
				FTimerDelegate::CreateUObject(this, &UPFGASC::OnEffectPeriodic, NewHandle),
				Effect->Period,
				true
			);
			EffectPeriodicTimerHandles.Add(NewHandle, PeriodicHandle);
		}
		else
		{
			ApplySnapshotToAttributeSet(Active);
		}
		break;
	}
	}
}

void UPFGASC::OnEffectPeriodic(int32 ActiveHandle)
{
	FActivePFGGameplayEffect* Active = FindActiveEffect(ActiveHandle);
	if (!Active)
	{
		if (UWorld* World = GetWorld())
		{
			if (FTimerHandle* Handle = EffectPeriodicTimerHandles.Find(ActiveHandle))
			{
				World->GetTimerManager().ClearTimer(*Handle);
			}
		}
		EffectPeriodicTimerHandles.Remove(ActiveHandle);
		return;
	}

	ApplySnapshotToAttributeSet(*Active);
}

void UPFGASC::OnEffectExpired(int32 ActiveHandle)
{
	RemoveActiveEffect(ActiveHandle);
}

bool UPFGASC::RemoveActiveEffect(int32 ActiveHandle)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return false;

	// (문제 5) O(1) 인덱스 조회
	const int32* IndexPtr = ActiveEffectHandleToIndex.Find(ActiveHandle);
	int32 FoundIndex = INDEX_NONE;

	if (IndexPtr && ActiveEffectsContainer.Items.IsValidIndex(*IndexPtr) &&
		ActiveEffectsContainer.Items[*IndexPtr].ActiveHandle == ActiveHandle)
	{
		FoundIndex = *IndexPtr;
	}
	else
	{
		for (int32 i = 0; i < ActiveEffectsContainer.Items.Num(); ++i)
		{
			if (ActiveEffectsContainer.Items[i].ActiveHandle == ActiveHandle)
			{
				FoundIndex = i;
				break;
			}
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		return false;
	}

	const FGameplayTagContainer GrantedTags = ActiveEffectsContainer.Items[FoundIndex].GrantedTags;
	if (!GrantedTags.IsEmpty())
	{
		RemoveGameplayTags(GrantedTags);
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();

		if (FTimerHandle* ExpireHandle = EffectExpirationTimerHandles.Find(ActiveHandle))
		{
			TM.ClearTimer(*ExpireHandle);
			EffectExpirationTimerHandles.Remove(ActiveHandle);
		}
		if (FTimerHandle* PeriodicHandle = EffectPeriodicTimerHandles.Find(ActiveHandle))
		{
			TM.ClearTimer(*PeriodicHandle);
			EffectPeriodicTimerHandles.Remove(ActiveHandle);
		}
	}

	ActiveEffectsContainer.Items.RemoveAt(FoundIndex);
	ActiveEffectsContainer.MarkArrayDirty();

	// (문제 5) 인덱스 맵 재구축 (RemoveAt으로 뒤쪽 인덱스 전부 -1 시프트되므로 전체 재구축이 가장 단순/안전)
	RebuildActiveEffectIndexMap();

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Infinite Effect 등 명시적 제거 API
/////////////////////////////////////////////////////////////////////////////

int32 UPFGASC::RemoveActiveEffectsByEffectID(int32 EffectID)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return 0;

	// RemoveActiveEffect가 배열을 수정하므로, 먼저 대상 핸들을 전부 모아둔 뒤 순회 제거.
	TArray<int32> HandlesToRemove;
	for (const FActivePFGGameplayEffect& Active : ActiveEffectsContainer.Items)
	{
		if (Active.EffectID == EffectID)
		{
			HandlesToRemove.Add(Active.ActiveHandle);
		}
	}

	int32 RemovedCount = 0;
	for (int32 Handle : HandlesToRemove)
	{
		if (RemoveActiveEffect(Handle))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}

int32 UPFGASC::RemoveActiveEffectsBySourceAbility(int32 SourceAbilityID)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority()) return 0;

	TArray<int32> HandlesToRemove;
	for (const FActivePFGGameplayEffect& Active : ActiveEffectsContainer.Items)
	{
		if (Active.SourceAbilityID == SourceAbilityID)
		{
			HandlesToRemove.Add(Active.ActiveHandle);
		}
	}

	int32 RemovedCount = 0;
	for (int32 Handle : HandlesToRemove)
	{
		if (RemoveActiveEffect(Handle))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}

void UPFGASC::GetActiveInfiniteEffects(TArray<FActivePFGGameplayEffect>& OutEffects) const
{
	OutEffects.Reset();
	for (const FActivePFGGameplayEffect& Active : ActiveEffectsContainer.Items)
	{
		if (Active.ExpirationTime < 0.f) // Infinite
		{
			OutEffects.Add(Active);
		}
	}
}