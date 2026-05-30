// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSGameState.h"
#include "Net/UnrealNetwork.h"

void ATPSGameState::OnRep_RemainingTime() {
	if (OnTimeUpdated.IsBound())
		OnTimeUpdated.Broadcast(RemainingTime);
}

void ATPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATPSGameState, RemainingTime);
}
