#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "EngineUtils.h"

UWeaponComponent::UWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UWeaponComponent::Fire(const FVector& MuzzleLocation)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, m_FireSound, MuzzleLocation);
    }
}