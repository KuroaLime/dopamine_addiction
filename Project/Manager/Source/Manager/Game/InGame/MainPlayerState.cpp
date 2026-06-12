// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerState.h"
#include "Net/UnrealNetwork.h"

AMainPlayerState::AMainPlayerState()
{
    
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMainPlayerState, weaponLV);
    DOREPLIFETIME(AMainPlayerState, playerLV);
}