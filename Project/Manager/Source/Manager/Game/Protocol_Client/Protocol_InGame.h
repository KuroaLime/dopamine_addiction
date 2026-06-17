// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Protocol_InGame.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None    = 0,
    HG      = 1,
    MG      = 2,
    SMG     = 3,
    AR      = 4,
    SR      = 5,
    SG      = 6,

};

UENUM(BlueprintType)
enum class EBulletType : uint8
{
    None = 0,
    General = 1,
};

UENUM(BlueprintType)
enum class ECardID : uint8
{
    None        = 0,
    Jan_Gwang   = 1,    Jan_Pi  = 2,
    Feb_Yul     = 3,    Feb_Ddi = 4,
    Mar_Gwang   = 5,    Mar_Ddi = 6,
    Apr_Yul     = 7,    Apr_Pi  = 8,
    May_Yul     = 9,    May_Ddi = 10,
    Jun_Yul     = 11,   Jun_Ddi = 12,
    Jul_Yul     = 13,   Jul_Ddi = 14,
    Aug_Gwang   = 15,   Aug_Yul = 16,
    Sep_Yul     = 17,   Sep_Ddi = 18,
    Oct_Gwang   = 19,   Oct_Yul = 20,
};

UENUM(BlueprintType)
enum class ECardMonthData : uint8
{
    None = 0,
    Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5,
    Jun = 6, Jul = 7, Aug = 8, Sep = 9, Oct = 10
};

UENUM(BlueprintType)
enum class ECardTypeData : uint8
{
    None = 0,
    Pi  = 1, Ddi    = 2,
    Yul = 3, Gwang  = 4,
};

//////////////////////////////////////////////////////////
// PlayerState Data
USTRUCT(BlueprintType)
struct FWeaponData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 LvDamage = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvFireRate = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvRange = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvMagazineCapacity = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvReloadTime = 0;
    UPROPERTY(BlueprintReadWrite)
    EBulletType bulletID = EBulletType::None;
    UPROPERTY(BlueprintReadWrite)
    EWeaponType weaponID = EWeaponType::None;
};

USTRUCT(BlueprintType)
struct FPlayerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 LvMovementSpeed = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvHealth = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 LvHealthRegeneration = 0;
};
//////////////////////////////////////////////////////////
// 실시간 플레이어 데이터
USTRUCT(BlueprintType)
struct FCurPlayerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 CurrentHP = 0;
    UPROPERTY(BlueprintReadWrite)
    int32 HoldingGold = 0;
};
//////////////////////////////////////////////////////////
// GameState Data
USTRUCT(BlueprintType)
struct FBulletDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBulletType bulletID = EBulletType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseDamage = 0;
};

USTRUCT(BlueprintType)
struct FWeaponDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeaponType weaponID = EWeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBulletType bulletID = EBulletType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BaseDamage = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BaseFireRate = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BaseRange = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BaseMagazineCapacity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BaseReloadTime = 0;
};

USTRUCT(BlueprintType)
struct FCardDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECardID cardID = ECardID::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECardMonthData cardMonth = ECardMonthData::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECardTypeData cardType = ECardTypeData::None;
};


UENUM(BlueprintType)
enum class ECardRuntimeState : uint8
{
    None = 0,
    WorldDrop = 1,
    Owned = 2,
    Used = 3,
    Removed = 4,
};

USTRUCT(BlueprintType)
struct FOwnedCardInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 CardInstanceId = 0;

    UPROPERTY(BlueprintReadOnly)
    ECardID CardID = ECardID::None;
};

USTRUCT(BlueprintType)
struct FPlayerDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseMovementSpeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseHealth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseHealthRegeneration = 0;
};