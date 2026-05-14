// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "Data/Lobby/Struct/ST_LobbySlot.h"
#include "GameFramework/GameStateBase.h"
#include "GS_Lobby.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyUpdated);

UCLASS()
class MANAGER_API AGS_Lobby : public AGameStateBase
{
	GENERATED_BODY()
public:
    AGS_Lobby();

    UPROPERTY(ReplicatedUsing = OnRep_LobbySlots, BlueprintReadWrite, Category = "Lobby")
    TArray<FLobbySlotData> LobbySlots;

    UFUNCTION()
    void OnRep_LobbySlots();

    UPROPERTY(BlueprintAssignable, Category = "Lobby")
    FOnLobbyUpdated OnLobbyUpdated;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
