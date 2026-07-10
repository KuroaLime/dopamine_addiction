// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Net/UnrealNetwork.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "PhaseGameStateInterface.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeUpdated, int32, NewTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundChanged, int32, NewRound);

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
	virtual FOnTimeUpdated& GetOnTimeUpdated() = 0;
	virtual FOnRoundChanged& GetOnRoundChanged() = 0;
	virtual void SetRemainingTime(int32 NewTime) = 0;
	virtual void BroadcastTimeUpdated(int32 NewTime) = 0;

	virtual void SetRoundWeapon(EWeaponType InWeaponID) = 0;
	virtual EWeaponType GetWeaponID() const = 0;
	virtual int32 GetWeaponBaseData(EWeaponType WeaponID, EWeaponBaseStatType StatType) const = 0;

	// 무기별 탄퍼짐/펠릿수/히트 판정 두께. DT_Weapon에 값이 없으면 SpreadAngle=0, PelletCount=1, TraceRadius=0(기존 라인 트레이스와 동일).
	virtual void GetWeaponFireProfile(EWeaponType WeaponID, float& OutSpreadAngle, int32& OutPelletCount, float& OutTraceRadius) const = 0;
};
