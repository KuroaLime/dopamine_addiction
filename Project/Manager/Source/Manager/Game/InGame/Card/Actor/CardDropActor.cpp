#include "Game/InGame/Card/Actor/CardDropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogManagerCard);

ACardDropActor::ACardDropActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(200000.0f));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    CardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh"));
    CardMesh->SetupAttachment(SceneRoot);
    // X축으로 90도 세운 자세 기준(실측): 로컬 X스케일 = 월드 너비, 로컬 Y스케일 = 월드 높이.
    CardMesh->SetRelativeScale3D(FVector(CardWidthScale, CardHeightScale, 1.0f));

    // 플레이어는 그냥 통과하되(Pawn 등 전 채널 Ignore), 배치 시 카드끼리 겹치지 않는지
    // 검사하는 IsCardDropLocationClear()가 쓰는 ECC_WorldDynamic 채널만 Block으로 살려둔다.
    CardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CardMesh->SetCollisionObjectType(ECC_WorldDynamic);
    CardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    CardMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMeshFinder.Succeeded())
    {
        CardMesh->SetStaticMesh(PlaneMeshFinder.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BackMaterialFinder(TEXT("/Game/InGame/CARD/M_CardDropBack.M_CardDropBack"));
    if (BackMaterialFinder.Succeeded())
    {
        CardMesh->SetMaterial(0, BackMaterialFinder.Object);
    }

    RefreshVisual();
}

void ACardDropActor::BeginPlay()
{
    Super::BeginPlay();
    LogClientReplicationOnce(TEXT("BeginPlay"));

    // 스폰 위치에서 결정론적으로 위상을 뽑아서, 리플리케이트 없이도 모든 클라이언트가
    // 이 카드에 대해 동일한(하지만 다른 카드와는 어긋난) 흔들림 타이밍을 계산하게 한다.
    const FVector SpawnLocation = GetActorLocation();
    FloatPhaseOffset = FMath::Fmod(
        FMath::Abs(SpawnLocation.X * 0.013f + SpawnLocation.Y * 0.029f),
        2.0f * PI);
}

void ACardDropActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CardMesh || bPickedUp)
    {
        return;
    }

    FloatElapsedSeconds += DeltaSeconds;

    const float AngularFrequency = (FloatPeriodSeconds > 0.0f) ? (2.0f * PI / FloatPeriodSeconds) : 0.0f;
    const float BobOffsetZ = FloatAmplitude * FMath::Sin(AngularFrequency * FloatElapsedSeconds + FloatPhaseOffset);

    // 기본 Plane을 X축으로 -90도 세운 다음, 그 위에 Z축 회전을 계속 곱해서 돌린다.
    const FQuat TiltXQuat(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(-90.0f));
    const FQuat SpinZQuat(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(FloatElapsedSeconds * SpinDegreesPerSecond));
    const FQuat FinalQuat = SpinZQuat * TiltXQuat;

    CardMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, BobOffsetZ), FinalQuat);
}

void ACardDropActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshVisual();
}

void ACardDropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACardDropActor, CardInstanceId);
    DOREPLIFETIME(ACardDropActor, CardID);
    DOREPLIFETIME(ACardDropActor, bPickedUp);
}

void ACardDropActor::InitCardDrop(int32 InCardInstanceId, ECardID InCardID)
{
    if (!HasAuthority())
    {
        return;
    }

    CardInstanceId = InCardInstanceId;
    CardID = InCardID;
    bPickedUp = false;
    RefreshVisual();
    ForceNetUpdate();
}

void ACardDropActor::MarkPickedUp()
{
    if (!HasAuthority())
    {
        return;
    }

    bPickedUp = true;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    ForceNetUpdate();
}

void ACardDropActor::OnRep_CardVisual()
{
    RefreshVisual();
    LogClientReplicationOnce(TEXT("OnRep_CardVisual"));
}

void ACardDropActor::LogClientReplicationOnce(const TCHAR* Context)
{
    if (HasAuthority() || bInitialReplicationLogged || CardInstanceId <= 0 || CardID == ECardID::None)
    {
        return;
    }

    bInitialReplicationLogged = true;

    const ULevel* ActorLevel = GetLevel();
    const FString LevelPackage = ActorLevel && ActorLevel->GetOutermost()
        ? ActorLevel->GetOutermost()->GetName()
        : TEXT("<NO_LEVEL>");

    UE_LOG(LogManagerCard, Display,
        TEXT("[CL] CardReplicated Instance=%d Card=%d Actor=%s Class=%s Location=%s Level=%s LocalRole=%d RemoteRole=%d Hidden=%d MeshVisible=%d Mesh=%s Collision=%d Context=%s"),
        CardInstanceId,
        static_cast<int32>(CardID),
        *GetName(),
        *GetNameSafe(GetClass()),
        *GetActorLocation().ToCompactString(),
        *LevelPackage,
        static_cast<int32>(GetLocalRole()),
        static_cast<int32>(GetRemoteRole()),
        IsHidden() ? 1 : 0,
        CardMesh && CardMesh->IsVisible() ? 1 : 0,
        *GetNameSafe(CardMesh ? CardMesh->GetStaticMesh().Get() : nullptr),
        CardMesh ? static_cast<int32>(CardMesh->GetCollisionEnabled()) : -1,
        Context ? Context : TEXT("<NULL>"));
}

void ACardDropActor::RefreshVisual()
{
    if (!CardMesh)
    {
        return;
    }

    CardMesh->SetVisibility(!bPickedUp, true);
    CardMesh->SetCollisionEnabled(bPickedUp ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
