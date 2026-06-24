#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Game/InGame/MainAnimInstance.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Game/InGame/MainPlayerState.h"
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

void UWeaponComponent::Multicast_PlayFireFeedback_Implementation(const FVector& MuzzleLocation)
{

    
    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), m_FireSound, MuzzleLocation);
    }
    AActor* WeaponActor = GetOwner();
    if (WeaponActor)
    {
        APawn* OwnerPawn = Cast<APawn>(WeaponActor->GetOwner());
        if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
        {
            if (UMainAnimInstance* MainAnim = Cast<UMainAnimInstance>(Character->GetMesh()->GetAnimInstance()))
            {
                if (AMainPlayerState* PS = OwnerPawn->GetPlayerState<AMainPlayerState>())
                {
                    if(WeaponType != PS->GetWeaponID())
                        WeaponType = PS->GetWeaponID();
                }
                MainAnim->PlayFireMontage(WeaponType);
            }
        }
    }
}
void UWeaponComponent::Reload()
{
    Multicast_PlayReloadFeedback();
}
void UWeaponComponent::Multicast_PlayReloadFeedback_Implementation()
{
    AActor* WeaponActor = GetOwner();
    if (WeaponActor)
    {
        APawn* OwnerPawn = Cast<APawn>(WeaponActor->GetOwner());
        if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
        {
            if (UMainAnimInstance* MainAnim = Cast<UMainAnimInstance>(Character->GetMesh()->GetAnimInstance()))
            {
                if (AMainPlayerState* PS = OwnerPawn->GetPlayerState<AMainPlayerState>())
                {
                    if (WeaponType != PS->GetWeaponID())
                        WeaponType = PS->GetWeaponID();
                }
                MainAnim->PlayReloadMontage(WeaponType);
            }
        }
    }
}