#include "Game/InGame/Card/Actor/CardDropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACardDropActor::ACardDropActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

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
