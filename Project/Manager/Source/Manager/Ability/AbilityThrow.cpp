// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityThrow.h"
#include "ManagerCharacter.h" // 캐릭터 헤더 필수
#include "Items/GrenadeItem.h"
#include "Kismet/GameplayStatics.h"

UAbilityThrow::UAbilityThrow()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Throw")));
}

bool UAbilityThrow::CanExecute() const
{
	// [DEBUG] 실행 조건 검사 시작
	// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, TEXT("[1] CanExecute Check Start"));

	// 1. 기본 체크
	if (!Super::CanExecute())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[CanExecute] Super Failed"));
		return false;
	}

	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);
	if (!Character)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[CanExecute] Character Cast Failed"));
		return false;
	}

	// 2. 인벤토리 확인
	bool bHasGrenade = false;
	for (ABaseItem* Item : Character->Inventory)
	{
		if (Item && Item->IsA(AGrenadeItem::StaticClass()))
		{
			bHasGrenade = true;
			// [DEBUG] 아이템 발견
			// UE_LOG(LogTemp, Log, TEXT("Grenade Found: %s"), *Item->GetName());
			break;
		}
	}

	if (!bHasGrenade)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[CanExecute] No Grenade in Inventory!"));
		return false;
	}

	// [DEBUG] 조건 통과
	// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[1] CanExecute Passed"));
	return true;
}

void UAbilityThrow::ActivateAbility()
{
	Super::ActivateAbility();

	// [DEBUG] 어빌리티 활성화 시작
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[2] ActivateAbility Start"));

	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);
	if (!Character)
	{
		EndAbility(true);
		return;
	}

	// 1. 아이템 캐싱
	CachedGrenadeItem = nullptr;
	for (ABaseItem* Item : Character->Inventory)
	{
		if (AGrenadeItem* Grenade = Cast<AGrenadeItem>(Item))
		{
			CachedGrenadeItem = Grenade;
			break;
		}
	}

	if (!CachedGrenadeItem)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Activate] Cached Item Failed!"));
		EndAbility(true);
		return;
	}

	// 2. 몽타주 재생
	if (ThrowMontage)
	{
		Character->PlayAnimMontage(ThrowMontage);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[Activate] Playing Montage..."));
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Activate] Warning: No Montage Assigned!"));
	}

	// 3. 타이머 설정
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ThrowTimerHandle,
			this,
			&UAbilityThrow::ExecuteThrow,
			ThrowDelay,
			false
		);

		FString Msg = FString::Printf(TEXT("[Activate] Timer Set: %.2f sec"), ThrowDelay);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, Msg);
	}
}

void UAbilityThrow::ExecuteThrow()
{
	// [DEBUG] 투척 실행 시작
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[3] ExecuteThrow Called (Timer End)"));

	AManagerCharacter* Character = Cast<AManagerCharacter>(OwnerCharacter);

	if (!Character)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Execute] Character Missing!"));
		EndAbility(true);
		return;
	}

	if (!CachedGrenadeItem || !CachedGrenadeItem->IsValidLowLevel())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Execute] Item is Invalid or Destroyed!"));
		EndAbility(true);
		return;
	}

	// 1. 위치 계산
	FVector SpawnLocation = Character->GetActorLocation();
	FRotator SpawnRotation = Character->GetControlRotation();

	if (Character->GetMesh()->DoesSocketExist(SocketName))
	{
		SpawnLocation = Character->GetMesh()->GetSocketLocation(SocketName);
		// [DEBUG] 소켓 확인
		// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Socket Found: %s"), *SocketName.ToString()));
	}
	else
	{
		SpawnLocation = Character->GetActorLocation() + (Character->GetActorForwardVector() * 50.0f) + FVector(0, 0, 50);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Execute] Socket Not Found! Using Fallback Location."));
	}

	// 2. 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;

	if (CachedGrenadeItem->ProjectileClassToSpawn)
	{
		AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
			CachedGrenadeItem->ProjectileClassToSpawn,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (SpawnedActor)
		{
			// [DEBUG] 스폰 성공
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[Execute] Projectile Spawn SUCCESS!"));
		}
		else
		{
			// [DEBUG] 스폰 실패
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[Execute] Projectile Spawn FAILED!"));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[Execute] Error: ProjectileClassToSpawn is NULL!"));
	}

	// 4. 인벤토리 제거
	Character->Inventory.Remove(CachedGrenadeItem);
	CachedGrenadeItem->Destroy();
	CachedGrenadeItem = nullptr;

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[4] Ability Finished Cleanly"));

	EndAbility(false);
}