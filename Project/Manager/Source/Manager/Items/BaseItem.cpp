// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/BaseItem.h"
#include "ManagerCharacter.h"

// Sets default values
ABaseItem::ABaseItem()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));


	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

// Called when the game starts or when spawned
void ABaseItem::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABaseItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABaseItem::OnPickedUp(AManagerCharacter* Player)
{
	if (!Player) return;

	Player->AddItemToInventory(this);

	if (MeshComp)
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	UE_LOG(LogTemp, Log, TEXT("Item Hidden and Added to Inventory: %s"), *GetName());
}

void ABaseItem::OnDropped(FVector DropLocation)
{
	if (!MeshComp) return;

	SetActorLocation(DropLocation);

	SetActorHiddenInGame(false);

	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetSimulatePhysics(true);
}
