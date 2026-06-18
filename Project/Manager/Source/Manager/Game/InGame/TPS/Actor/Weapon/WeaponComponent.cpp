#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "EngineUtils.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"

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
    AActor* WeaponActor = GetOwner();
    APawn* ShooterPawn = WeaponActor ? Cast<APawn>(WeaponActor->GetOwner()) : nullptr;

    FVector AimDirection = WeaponActor ? WeaponActor->GetActorForwardVector() : FVector::ForwardVector;
    if (ShooterPawn)
    {
        if (AController* Controller = ShooterPawn->GetController())
        {
            AimDirection = Controller->GetControlRotation().Vector();
        }
        else
        {
            AimDirection = ShooterPawn->GetActorForwardVector();
        }
    }

    Fire(MuzzleLocation, MuzzleLocation + AimDirection * MaxRange, Damage);
}

void UWeaponComponent::Fire(const FVector& MuzzleLocation, const FVector& ShotDirection, float InDamage)
{
    AActor* WeaponActor = GetOwner();
    UWorld* World = GetWorld();
    if (!WeaponActor || !WeaponActor->HasAuthority() || !World)
    {
        return;
    }

    APawn* ShooterPawn = Cast<APawn>(WeaponActor->GetOwner());
    AController* InstigatorController = ShooterPawn ? ShooterPawn->GetController() : nullptr;
    const FVector End = ShotDirection;
    const float AppliedDamage = InDamage >= 0.0f ? InDamage : Damage;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ServerWeaponFire), false);
    Params.AddIgnoredActor(WeaponActor);
    if (ShooterPawn)
    {
        Params.AddIgnoredActor(ShooterPawn);
    }

    auto ApplyHitToPawn = [&](APawn* TargetPawn, const FVector& HitLocation, const TCHAR* Method) -> bool
    {
        if (!TargetPawn || TargetPawn == ShooterPawn)
        {
            return false;
        }

        UGameplayStatics::ApplyDamage(TargetPawn, AppliedDamage, InstigatorController, WeaponActor, nullptr);
        DrawDebugBox(World, HitLocation, FVector(8.0f, 8.0f, 8.0f), FColor::Green, false, 1.0f);

        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireHit Method=%s Weapon=%s Target=%s Damage=%.2f HitLoc=%s"),
            Method,
            *WeaponActor->GetName(),
            *TargetPawn->GetName(),
            AppliedDamage,
            *HitLocation.ToCompactString());
        return true;
    };

    DrawDebugLine(World, MuzzleLocation, End, FColor::Red, false, 1.0f, 0, 1.0f);

    TArray<FHitResult> LineHits;
    World->LineTraceMultiByChannel(LineHits, MuzzleLocation, End, ECC_Visibility, Params);

    FHitResult FirstBlockingWorldHit;
    bool bHasWorldHit = false;

    for (const FHitResult& Hit : LineHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || HitActor == WeaponActor || HitActor == ShooterPawn)
        {
            continue;
        }

        if (APawn* HitPawn = Cast<APawn>(HitActor))
        {
            if (ApplyHitToPawn(HitPawn, Hit.ImpactPoint, TEXT("Line")))
            {
                return;
            }
        }

        if (Hit.bBlockingHit && !bHasWorldHit)
        {
            FirstBlockingWorldHit = Hit;
            bHasWorldHit = true;
        }
    }

    TArray<FHitResult> SweepHits;
    FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(ServerWeaponFireSweep), false);
    SweepParams.AddIgnoredActor(WeaponActor);
    if (ShooterPawn)
    {
        SweepParams.AddIgnoredActor(ShooterPawn);
    }

    World->SweepMultiByChannel(SweepHits, MuzzleLocation, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(80.0f), SweepParams);
    for (const FHitResult& Hit : SweepHits)
    {
        APawn* HitPawn = Cast<APawn>(Hit.GetActor());
        if (ApplyHitToPawn(HitPawn, Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint, TEXT("Sweep")))
        {
            return;
        }
    }

    APawn* BestPawn = nullptr;
    FVector BestPoint = FVector::ZeroVector;
    float BestAlong = TNumericLimits<float>::Max();
    const FVector ShotVector = End - MuzzleLocation;
    const float ShotLengthSq = ShotVector.SizeSquared();

    if (ShotLengthSq > KINDA_SMALL_NUMBER)
    {
        for (TActorIterator<APawn> It(World); It; ++It)
        {
            APawn* Candidate = *It;
            if (!Candidate || Candidate == ShooterPawn)
            {
                continue;
            }

            const FVector CandidateCenter = Candidate->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
            const float T = FVector::DotProduct(CandidateCenter - MuzzleLocation, ShotVector) / ShotLengthSq;
            if (T < 0.0f || T > 1.0f)
            {
                continue;
            }

            const FVector ClosestPoint = MuzzleLocation + ShotVector * T;
            if (FVector::DistSquared(CandidateCenter, ClosestPoint) > FMath::Square(180.0f))
            {
                continue;
            }

            FHitResult BlockHit;
            FCollisionQueryParams BlockParams(SCENE_QUERY_STAT(ServerWeaponFireAssistBlock), false);
            BlockParams.AddIgnoredActor(WeaponActor);
            if (ShooterPawn)
            {
                BlockParams.AddIgnoredActor(ShooterPawn);
            }

            const bool bBlocked = World->LineTraceSingleByChannel(BlockHit, MuzzleLocation, CandidateCenter, ECC_Visibility, BlockParams)
                && BlockHit.GetActor()
                && BlockHit.GetActor() != Candidate;

            if (bBlocked)
            {
                continue;
            }

            if (T < BestAlong)
            {
                BestAlong = T;
                BestPawn = Candidate;
                BestPoint = CandidateCenter;
            }
        }
    }

    if (ApplyHitToPawn(BestPawn, BestPoint, TEXT("Assist")))
    {
        return;
    }

    if (bHasWorldHit && FirstBlockingWorldHit.GetActor())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireWorldHit Weapon=%s Target=%s Damage=%.2f HitLoc=%s"),
            *WeaponActor->GetName(),
            *FirstBlockingWorldHit.GetActor()->GetName(),
            AppliedDamage,
            *FirstBlockingWorldHit.ImpactPoint.ToCompactString());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireMiss Weapon=%s Damage=%.2f Start=%s End=%s"),
        *WeaponActor->GetName(),
        AppliedDamage,
        *MuzzleLocation.ToCompactString(),
        *End.ToCompactString());
}

void UWeaponComponent::PlayLocalFireFeedback()
{
    AActor* WeaponActor = GetOwner();
    if (m_FireSound && WeaponActor)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), m_FireSound, WeaponActor->GetActorLocation());
    }
}

void UWeaponComponent::FireOnce()
{
    AActor* WeaponActor = GetOwner();
    if (!WeaponActor)
    {
        return;
    }

    PlayLocalFireFeedback();

    APawn* OwnerPawn = Cast<APawn>(WeaponActor->GetOwner());
    FVector ViewLocation = WeaponActor->GetActorLocation();
    FRotator ViewRotation = WeaponActor->GetActorRotation();

    if (OwnerPawn)
    {
        if (AController* Controller = OwnerPawn->GetController())
        {
            Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        }
        else
        {
            ViewLocation = OwnerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
            ViewRotation = OwnerPawn->GetActorRotation();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[Client] TPS FireOnce Weapon=%s OwnerPawn=%s ViewLoc=%s ViewRot=%s"),
        *WeaponActor->GetName(),
        OwnerPawn ? *OwnerPawn->GetName() : TEXT("<NULL>"),
        *ViewLocation.ToCompactString(),
        *ViewRotation.ToCompactString());

    if (WeaponActor->HasAuthority())
    {
        ExecuteServerFireFromView(ViewLocation, ViewRotation);
    }
    else
    {
        Server_FireFromClient(ViewLocation, ViewRotation);
    }
}

void UWeaponComponent::Server_FireFromClient_Implementation(FVector ViewLocation, FRotator ViewRotation)
{
    ExecuteServerFireFromView(ViewLocation, ViewRotation);
}

void UWeaponComponent::ExecuteServerFireFromView(FVector ViewLocation, FRotator ViewRotation)
{
    AActor* WeaponActor = GetOwner();
    UWorld* World = GetWorld();
    if (!WeaponActor || !World)
    {
        return;
    }

    AMainGameMode* MainGM = World->GetAuthGameMode<AMainGameMode>();
    if (!MainGM || !MainGM->IsBattleRoyalePhase())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=InvalidPhase Weapon=%s"), *WeaponActor->GetName());
        return;
    }

    APawn* ShooterPawn = Cast<APawn>(WeaponActor->GetOwner());
    if (!ShooterPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireRejected Reason=MissingOwnerPawn Weapon=%s"), *WeaponActor->GetName());
        return;
    }

    const FVector ViewEnd = ViewLocation + ViewRotation.Vector() * MaxRange;

    FCollisionQueryParams ViewParams(SCENE_QUERY_STAT(ServerWeaponClientViewTrace), false);
    ViewParams.AddIgnoredActor(WeaponActor);
    ViewParams.AddIgnoredActor(ShooterPawn);

    FHitResult ViewHit;
    const bool bViewHit = World->LineTraceSingleByChannel(ViewHit, ViewLocation, ViewEnd, ECC_Visibility, ViewParams);
    const FVector TargetPoint = bViewHit ? ViewHit.ImpactPoint : ViewEnd;

    FVector MuzzleLoc = WeaponActor->GetActorLocation();
    if (AWeapon* Weapon = Cast<AWeapon>(WeaponActor))
    {
        if (Weapon->m_pMesh)
        {
            MuzzleLoc = Weapon->m_pMesh->DoesSocketExist(TEXT("Muzzle"))
                ? Weapon->m_pMesh->GetSocketLocation(TEXT("Muzzle"))
                : Weapon->m_pMesh->GetComponentLocation();
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] TPS FireAccepted Owner=%s Weapon=%s Damage=%.2f Range=%.2f ViewLoc=%s ViewRot=%s Target=%s ViewHit=%s"),
        *ShooterPawn->GetName(),
        *WeaponActor->GetName(),
        Damage,
        MaxRange,
        *ViewLocation.ToCompactString(),
        *ViewRotation.ToCompactString(),
        *TargetPoint.ToCompactString(),
        bViewHit && ViewHit.GetActor() ? *ViewHit.GetActor()->GetName() : TEXT("<None>"));

    Fire(MuzzleLoc, TargetPoint, Damage);
}

bool UWeaponComponent::Server_FireFromClient_Validate(FVector ViewLocation, FRotator ViewRotation)
{
    return true;
}

void UWeaponComponent::StartLoopFire()
{
    GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UWeaponComponent::FireOnce, FireInterval, true);
}

void UWeaponComponent::StopLoopFire()
{
    GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void UWeaponComponent::Reload()
{
    if (m_FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), m_FireSound, GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
    }
}
