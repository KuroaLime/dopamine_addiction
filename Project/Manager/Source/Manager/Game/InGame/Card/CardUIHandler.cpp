// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/CardUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

UCardUIHandler::UCardUIHandler()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCardUIHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UCardUIHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UCardUIHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCardUIHandler::UIActivate()
{
	Super::UIActivate();
}

void UCardUIHandler::UIDeactivate()
{
	Super::UIDeactivate();
}
