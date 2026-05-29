// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Actor/CustomItem.h"

// Sets default values
ACustomItem::ACustomItem()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// 기본 설정: 물리 켜짐 (바닥에 굴러다님)
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));

}

// Called when the game starts or when spawned
void ACustomItem::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ACustomItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ACustomItem::OnPickedUp()
{
	if (!MeshComp) return;

	// 1. 물리 끄고 충돌 끄기
	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 2. 눈에서 안 보이게 (인벤토리 들어간 척)
	SetActorHiddenInGame(true);
}

void ACustomItem::OnDropped(FVector DropLocation)
{
	if (!MeshComp) return;

	// 1. 플레이어 앞으로 위치 이동
	SetActorLocation(DropLocation);

	// 2. 다시 보이게
	SetActorHiddenInGame(false);

	// 3. 물리 켜고 충돌 켜기
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetSimulatePhysics(true);
}

