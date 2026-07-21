#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
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
#include "TimerManager.h"
#include "Engine/World.h"
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
    DOREPLIFETIME(UWeaponComponent, bIsReloading);
}

void UWeaponComponent::StartReloadLock(float Duration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    bIsReloading = true;
    if (Duration > 0.f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(ReloadLockTimerHandle, this, &UWeaponComponent::ClearReloadLock, Duration, false);
    }
    else
    {
        ClearReloadLock();
    }
}

void UWeaponComponent::CancelReloadLock()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReloadLockTimerHandle);
    }
    ClearReloadLock();
    GetOwner()->ForceNetUpdate();
}

void UWeaponComponent::ClearReloadLock()
{
    bIsReloading = false;
}
void UWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentAmmo = GetMaxMagazineCapacity();
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
    FVector ActualMuzzleLoc = MuzzleLocation;
    AWeapon* Weapon = Cast<AWeapon>(GetOwner());
    if (Weapon && Weapon->m_pMesh)
    {
        ActualMuzzleLoc = Weapon->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
    }

    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), m_FireSound, ActualMuzzleLoc);
    }

    // 총알 트레이서: 총구에서 명중점 방향으로 회전시켜 스폰.
    // (NS_BulletTracer가 Local Space + 로컬 +X 속도라, 이 회전이 곧 날아가는 방향이 됨)
    if (BulletTracerFX && GetWorld())
    {
        const FRotator AimRot = (TargetLocation - ActualMuzzleLoc).Rotation();
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BulletTracerFX, ActualMuzzleLoc, AimRot);
    }
    if (UMainAnimInstance* MainAnim = ResolveOwnerAnimAndSyncWeapon())
    {
        MainAnim->PlayFireMontage(WeaponType);
    }
}

UMainAnimInstance* UWeaponComponent::ResolveOwnerAnimAndSyncWeapon()
{
    AActor* WeaponActor = GetOwner();
    if (!WeaponActor) return nullptr;

    APawn* OwnerPawn = Cast<APawn>(WeaponActor->GetOwner());
    ACharacter* Character = Cast<ACharacter>(OwnerPawn);
    if (!Character) return nullptr;

    UMainAnimInstance* MainAnim = Cast<UMainAnimInstance>(Character->GetMesh()->GetAnimInstance());
    if (!MainAnim) return nullptr;

    // 멀티 동기화 진실원본인 PlayerState의 무기 타입으로 맞춤
    if (AMainPlayerState* PS = OwnerPawn->GetPlayerState<AMainPlayerState>())
    {
        if (WeaponType != PS->GetWeaponID())
        {
            WeaponType = PS->GetWeaponID();
        }
    }
    return MainAnim;
}

void UWeaponComponent::Multicast_PlayReloadFeedback_Implementation()
{
    if (UMainAnimInstance* MainAnim = ResolveOwnerAnimAndSyncWeapon())
    {
        MainAnim->PlayReloadMontage(WeaponType);
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
