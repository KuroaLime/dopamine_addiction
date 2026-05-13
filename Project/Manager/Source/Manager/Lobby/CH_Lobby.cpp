// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/CH_Lobby.h"
#include "Camera/CameraComponent.h"

// Sets default values
ACH_Lobby::ACH_Lobby()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	LobbyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));
	LobbyCamera->SetupAttachment(RootComponent);

}

// Called when the game starts or when spawned
void ACH_Lobby::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACH_Lobby::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACH_Lobby::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

