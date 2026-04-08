// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/CSM_Componenet.h"

// Sets default values for this component's properties
UCSM_Componenet::UCSM_Componenet()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCSM_Componenet::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCSM_Componenet::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

