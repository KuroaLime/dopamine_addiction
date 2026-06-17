// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "PhasePlayerStateInterface.generated.h"

UENUM(BlueprintType)
enum class EWeaponStatType : uint8
{
	None = 0,
	Damage = 1,
	FireRate = 2,
	Range = 3,
	MagazineCapacity = 4,
	ReloadTime = 5,
};

UENUM(BlueprintType)
enum class EPlayerStatType : uint8
{
	None = 0,
	MovementSpeed = 1,
	Health = 2,
	HealthRegeneration = 3,
};

UENUM(BlueprintType)
enum class ECurPlayerStatType : uint8
{
	None = 0,
	CurrentHP = 1,
	HoldingGold = 2,
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPhasePlayerStateInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MANAGER_API IPhasePlayerStateInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetWeaponStatLV(EWeaponStatType StatType) const = 0;
	virtual int32 GetPlayerStatLV(EPlayerStatType StatType) const = 0;
	virtual int32 GetCurPlayerStatLV(ECurPlayerStatType StatType) const = 0;

	virtual EBulletType GetBulletID() const = 0;
	virtual EWeaponType GetWeaponID() const = 0;

	virtual void SetWeaponID(EWeaponType WeaponID) = 0;
};
