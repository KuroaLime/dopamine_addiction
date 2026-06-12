// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "MainPlayerState.generated.h"

UCLASS()
class MANAGER_API AMainPlayerState : public APlayerState,
									 public IPhasePlayerStateInterface
{
	GENERATED_BODY()
	
public:
	AMainPlayerState();

public:
	virtual int32 GetWeaponStatLV(EWeaponStatType StatType) const override;
	virtual int32 GetPlayerStatLV(EPlayerStatType StatType) const override;
	virtual EBulletType GetBulletID() const override;
	virtual EWeaponType GetWeaponID() const override;

	virtual void SetWeaponID(EWeaponType WeaponID) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon Data")
	FWeaponData WeaponData;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Data")
	FPlayerData PlayerData;
};
