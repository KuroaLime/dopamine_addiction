// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CustomASC.generated.h"

class UCustomAbility;
class AActor;

USTRUCT(BlueprintType)
struct FCustomGameplayEventData
{
	GENERATED_BODY()

public:
	// 이벤트를 발생시킨 주체 (예: 아이템)
	UPROPERTY(BlueprintReadWrite)
	AActor* Instigator = nullptr;

	// 전달하고 싶은 임의의 액터 데이터 (예: 주워진 아이템 자신)
	UPROPERTY(BlueprintReadWrite)
	AActor* TargetObject = nullptr;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UCustomASC : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCustomASC();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void GiveAbility(TSubclassOf<UCustomAbility> AbilityClass);
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);
	void AddGameplayTags(const FGameplayTagContainer& TagsToAdd);
	void RemoveGameplayTags(const FGameplayTagContainer& TagsToRemove);
	bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const;
	void CancelAbilitiesWithTag(const FGameplayTagContainer& TagsToCancel);
	bool HandleGameplayEvent(FGameplayTag EventTag, const FCustomGameplayEventData& Payload);
protected:
	// 배운 어빌리티 목록 (실체화된 객체들)
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TArray<UCustomAbility*> ActivatableAbilities;

	// 현재 캐릭터가 가진 태그 목록 (버프, 상태이상, 쿨타임 등)
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	FGameplayTagContainer OwnedTags;

public:
	// 클라이언트가 서버에 타격 정보를 보내는 통로
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_ProcessHit(const FHitResult& HitResult);
};
