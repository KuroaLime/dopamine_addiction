// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"

// Sets default values
AA_Spawn::AA_Spawn()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BODY"));
	RootComponent = Body;
}

// Called when the game starts or when spawned
void AA_Spawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AA_Spawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

