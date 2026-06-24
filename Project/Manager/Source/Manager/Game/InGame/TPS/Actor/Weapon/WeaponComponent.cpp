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
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
UWeaponComponent::UWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);

    // 기본 총알 트레이서 (BP에서 무기별로 덮어쓸 수 있음)
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BulletTracerAsset(TEXT("/Game/Character/Weapons/NS_BulletTracer.NS_BulletTracer"));
    if (BulletTracerAsset.Succeeded())
    {
        BulletTracerFX = BulletTracerAsset.Object;
    }
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
void UWeaponComponent::Multicast_PlayFireFeedback_Implementation(const FVector& MuzzleLocation, const FVector& TargetLocation)
{
    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), m_FireSound, MuzzleLocation);
    }

    // 총알 트레이서: 총구에서 명중점 방향으로 회전시켜 스폰.
    // (NS_BulletTracer가 Local Space + 로컬 +X 속도라, 이 회전이 곧 날아가는 방향이 됨)
    if (BulletTracerFX && GetWorld())
    {
        const FRotator AimRot = (TargetLocation - MuzzleLocation).Rotation();
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BulletTracerFX, MuzzleLocation, AimRot);
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
