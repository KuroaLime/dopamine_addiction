// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "MainPlayerState.generated.h"

UCLASS()
class MANAGER_API AMainPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	AMainPlayerState();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Data")
	FWeaponData weaponLV;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Data")
	FPlayerData playerLV;
};
