// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/CustomASC.h"
#include "Default/Ability/CustomAbility.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"

// Sets default values for this component's properties
UCustomASC::UCustomASC()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UCustomASC::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCustomASC::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UCustomASC::GiveAbility(TSubclassOf<UCustomAbility> AbilityClass)
{
	if (!AbilityClass) return;

	UCustomAbility* NewAbility = NewObject<UCustomAbility>(this, AbilityClass);

	if (NewAbility)
	{
		NewAbility->InitializeAbility(this);

		ActivatableAbilities.Add(NewAbility);

		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Black, TEXT("Skill Granted!"));
	}
}

bool UCustomASC::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{

	FString TargetTagName = AbilityTag.ToString();

	for (UCustomAbility* Ability : ActivatableAbilities)
	{
		if (Ability)
		{
			if (Ability->GetAbilityTags().HasTag(AbilityTag))
			{
				return Ability->TryActivateAbility();
			}
		}
	}
	return false; 
}

// [3] 태그 관리 구현
void UCustomASC::AddGameplayTags(const FGameplayTagContainer& TagsToAdd)
{
	OwnedTags.AppendTags(TagsToAdd);
}

void UCustomASC::RemoveGameplayTags(const FGameplayTagContainer& TagsToRemove)
{
	OwnedTags.RemoveTags(TagsToRemove);
}

bool UCustomASC::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	// 내 OwnedTags 중에, 인자로 들어온 TagContainer와 하나라도 겹치는 게 있나?
	return OwnedTags.HasAny(TagContainer);
}
void UCustomASC::CancelAbilitiesWithTag(const FGameplayTagContainer& TagsToCancel)
{
	if (TagsToCancel.IsEmpty()) return;

	for (UCustomAbility* Ability : ActivatableAbilities)
	{
		if (Ability)
		{
			// 어빌리티가 가진 태그들 중 하나라도 취소 대상 태그와 겹치는지 확인
			if (Ability->GetAbilityTags().HasAny(TagsToCancel))
			{
				// 조건에 맞으면 어빌리티 강제 종료
				Ability->CancelAbility();
			}
		}
	}
}

bool UCustomASC::HandleGameplayEvent(FGameplayTag EventTag, const FCustomGameplayEventData& Payload)
{
	// 등록된 어빌리티들을 순회하며 해당 태그(EventTag)를 가진 어빌리티를 찾음
	for (UCustomAbility* Ability : ActivatableAbilities)
	{
		if (Ability && Ability->GetAbilityTags().HasTag(EventTag))
		{
			// 해당 어빌리티를 실행하면서 데이터(Payload)도 같이 넘겨줌
			// (주의: UCustomAbility 클래스에 이 함수를 새로 만들어주셔야 합니다!)
			return Ability->TryActivateAbilityWithEvent(Payload);
		}
	}
	return false;
}

void UCustomASC::ServerRPC_ProcessHit_Implementation(const FHitResult& HitResult)
{
	// 1. 타격 대상 확인
	AActor* HitActor = HitResult.GetActor();
	if (!HitActor) return;

	// 2. 데미지를 주는 주체(나 자신) 가져오기
	// UActorComponent를 상속받았다면 GetOwner()는 무조건 작동합니다.
	AActor* MyCharacter = GetOwner();

	// 3. 실제 데미지 적용
	UGameplayStatics::ApplyDamage(
		HitActor,
		20.0f,
		nullptr,
		MyCharacter, // 'OwnerASC'가 아니라 'MyCharacter'를 넣어야 합니다.
		nullptr
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Server: Hit Confirmed!"));
	}
}

// Validation 함수도 잊지 마세요.
bool UCustomASC::ServerRPC_ProcessHit_Validate(const FHitResult& HitResult)
{
	return true;
}