// Fill out your copyright notice in the Description page of Project Settings.


#include "ManagerGameState.h"
#include "Net/UnrealNetwork.h"

void AManagerGameState::OnRep_RemainingTime() {
	if (OnTimeUpdated.IsBound())
		OnTimeUpdated.Broadcast(RemainingTime);
}

void AManagerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AManagerGameState, Table);
	DOREPLIFETIME(AManagerGameState, RemainingTime);
}