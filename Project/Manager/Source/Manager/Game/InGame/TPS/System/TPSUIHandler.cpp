// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSUIHandler.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"

UTPSUIHandler::UTPSUIHandler()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTPSUIHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UTPSUIHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UTPSUIHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTPSUIHandler::UIActivate()
{
	Super::UIActivate();
}

void UTPSUIHandler::UIDeactivate()
{
	Super::UIDeactivate();
}