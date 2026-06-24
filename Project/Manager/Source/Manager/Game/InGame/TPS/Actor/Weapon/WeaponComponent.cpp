#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Game/InGame/MainAnimInstance.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Game/InGame/MainPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"  
UWeaponComponent::UWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWeaponComponent, CurrentAmmo);
}
void UWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentAmmo = MaxMagazineCapacity;
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}


void UWeaponComponent::ConsumeAmmo()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
    }
}
void UWeaponComponent::Multicast_PlayFireFeedback_Implementation(const FVector& MuzzleLocation)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
            FString::Printf(TEXT("[Debug] Fire Called! Ammo: %d/%d, HasAuthority: %d"),
                CurrentAmmo, MaxMagazineCapacity, (GetOwner() && GetOwner()->HasAuthority()) ? 1 : 0));
    }

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
                    {
                        WeaponType = PS->GetWeaponID();
                    }
                }
                MainAnim->PlayReloadMontage(WeaponType);
            }
        }
    }
}

void UWeaponComponent::SetCurrentAmmo(int32 NewAmmo)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CurrentAmmo = NewAmmo;
    }
}

int32 UWeaponComponent::GetMaxMagazineCapacity() const
{
    AActor* WeaponActor = GetOwner();
    if (WeaponActor)
    {
        APawn* OwnerPawn = Cast<APawn>(WeaponActor->GetOwner());
        if (AMainPlayerState* PS = OwnerPawn ? OwnerPawn->GetPlayerState<AMainPlayerState>() : nullptr)
        {
            if (AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
            {
                if (IPhaseGameStateInterface* PhaseGS = Cast<IPhaseGameStateInterface>(GS))
                {
                    int32 BaseCapacity = PhaseGS->GetWeaponBaseData(WeaponType, EWeaponBaseStatType::MagazineCapacity);
                    if (BaseCapacity > 0)
                    {
                        return PS->GetFinalMaxMagazine(BaseCapacity);
                    }
                }
            }
        }
    }
    return MaxMagazineCapacity;
}
