// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Default/Ability/Interface/AbilitySystemInterface.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
UWeaponComponent::UWeaponComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UWeaponComponent::Fire(const FVector& MuzzleLocation, const FVector& TargetLocation, float InDamage) {
    AActor* Owner = GetOwner();
    if (!Owner) return;

    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, m_FireSound, MuzzleLocation);
    }

    if (!Owner->HasAuthority() || InDamage < 0.0f || TargetLocation.IsNearlyZero())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    if (AActor* WeaponOwner = Owner->GetOwner())
    {
        Params.AddIgnoredActor(WeaponOwner);
    }

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, MuzzleLocation, TargetLocation, ECC_Visibility, Params);

    if (bHit && Hit.GetActor())
    {
        AActor* TargetActor = Hit.GetActor();
        if (Cast<APawn>(TargetActor))
        {
            UGameplayStatics::ApplyDamage(TargetActor, InDamage, nullptr, Owner, nullptr);
            UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireHit Weapon=%s Target=%s Damage=%.2f"), *Owner->GetName(), *TargetActor->GetName(), InDamage);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireWorldHit Weapon=%s Target=%s Damage=%.2f"), *Owner->GetName(), *TargetActor->GetName(), InDamage);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireMiss Weapon=%s Damage=%.2f"), *Owner->GetName(), InDamage);
    }
}

void UWeaponComponent::FireOnce() {
	IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()->GetOwner());
	if (!Owner) return;

	if (UPFGASC* ASC = Owner->GetASC())
	{
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));
	}
}

void UWeaponComponent::StartLoopFire(){

	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UWeaponComponent::FireOnce, FireInterval, true);
}

void UWeaponComponent::StopLoopFire() {
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void UWeaponComponent::Reload() {
	if (m_FireSound) {
		//사운드 플레이
		//나이아가라 이펙트 추가
		//애니메이션
	}
}