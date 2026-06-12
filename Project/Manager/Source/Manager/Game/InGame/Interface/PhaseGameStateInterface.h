// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "PhaseGameStateInterface.generated.h"

UENUM(BlueprintType)
enum class EWeaponBaseStatType : uint8
{
	None = 0,
	Damage = 1,
	FireRate = 2,
	Range = 3,
	MagazineCapacity = 4,
	ReloadTime = 5,
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPhaseGameStateInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MANAGER_API IPhaseGameStateInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual int32 GetRemainingTime() const = 0;
	virtual void SetRemainingTime(int32 NewTime) = 0;
	virtual void BroadcastTimeUpdated(int32 NewTime) = 0;

	virtual void SetRoundWeapon(EWeaponType InWeaponID) = 0;
	virtual EWeaponType GetWeaponID() const = 0;
	virtual int32 GetWeaponBaseData(EWeaponType WeaponID, EWeaponBaseStatType StatType) const = 0;
};
