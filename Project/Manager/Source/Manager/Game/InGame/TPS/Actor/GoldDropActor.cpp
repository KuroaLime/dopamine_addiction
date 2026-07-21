#include "Game/InGame/TPS/Actor/GoldDropActor.h"

#include "Manager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/InGame/MainPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AGoldDropActor::AGoldDropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(5.0f);
	SetMinNetUpdateFrequency(2.0f);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	RootComponent = PickupSphere;
	PickupSphere->InitSphereRadius(90.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&AGoldDropActor::OnPickupSphereBeginOverlap);

	GoldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoldMesh"));
	GoldMesh->SetupAttachment(PickupSphere);
	GoldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GoldMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.12f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		GoldMesh->SetStaticMesh(CylinderMeshFinder.Object);
	}
}

void AGoldDropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGoldDropActor, GoldAmount);
}

void AGoldDropActor::InitGoldDrop(
	int32 InGoldAmount,
	AMainPlayerState* InSourcePlayerState,
	float SourcePickupLockSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	GoldAmount = FMath::Max(0, InGoldAmount);
	SourcePlayerState = InSourcePlayerState;
	SourcePickupUnlockTimeSeconds = GetWorld()
		? GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, SourcePickupLockSeconds)
		: 0.0;
	bCollected = false;

	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->UpdateOverlaps();
	ForceNetUpdate();
}

void AGoldDropActor::OnPickupSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bCollected || GoldAmount <= 0)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	AMainPlayerState* Collector = Pawn
		? Pawn->GetPlayerState<AMainPlayerState>()
		: nullptr;
	if (!Collector || Collector->CurPlayerData.CurrentHP <= 0)
	{
		return;
	}

	if (Collector == SourcePlayerState.Get() &&
		GetWorld() &&
		GetWorld()->GetTimeSeconds() < SourcePickupUnlockTimeSeconds)
	{
		return;
	}

	bCollected = true;
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collector->AddGold(GoldAmount);

	UE_LOG(LogManager, Display,
		TEXT("[DS] GoldDrop Collected Player=%s Amount=%d Location=%s"),
		*Collector->GetPlayerName(),
		GoldAmount,
		*GetActorLocation().ToCompactString());

	Destroy();
}
