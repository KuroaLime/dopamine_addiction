#include "Game/InGame/Card/Actor/CardDropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogManagerCard);

ACardDropActor::ACardDropActor()
{
    PrimaryActorTick.bCanEverTick = false;
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
    CardMesh->SetRelativeScale3D(FVector(0.8f, 1.1f, 0.04f));
    CardMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CardMesh->SetCollisionObjectType(ECC_WorldDynamic);
    CardMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshFinder.Succeeded())
    {
        CardMesh->SetStaticMesh(CubeMeshFinder.Object);
    }

    RefreshVisual();
}

void ACardDropActor::BeginPlay()
{
    Super::BeginPlay();
    LogClientReplicationOnce(TEXT("BeginPlay"));
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
    CardMesh->SetCollisionEnabled(bPickedUp ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}
