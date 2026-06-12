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
enum class ECardMonth_ex : uint8
{
    None = 0,
    Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5,
    Jun = 6, Jul = 7, Aug = 8, Sep = 9, Oct = 10
};

//////////////////////////////////////////////////////////
// PlayerState Data
USTRUCT(BlueprintType)
struct FWeaponData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 lvDamage;
    UPROPERTY(BlueprintReadWrite)
    int32 lvFireRate;
    UPROPERTY(BlueprintReadWrite)
    int32 lvRange;
    UPROPERTY(BlueprintReadWrite)
    int32 lvMagazineCapacity;
    UPROPERTY(BlueprintReadWrite)
    int32 lvReloadTime;
    UPROPERTY(BlueprintReadWrite)
    EBulletType bulletID;
    UPROPERTY(BlueprintReadWrite)
    EWeaponType weaponID;
};

USTRUCT(BlueprintType)
struct FPlayerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 lvMovementSpeed;
    UPROPERTY(BlueprintReadWrite)
    int32 lvHealth;
    UPROPERTY(BlueprintReadWrite)
    int32 lvHealthRegeneration;
};

//////////////////////////////////////////////////////////
// GameState Data
USTRUCT(BlueprintType)
struct FBulletDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBulletType bulletID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseDamage;
};

USTRUCT(BlueprintType)
struct FWeaponDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeaponType weaponID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBulletType bulletID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseDamage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseRange;
};

USTRUCT(BlueprintType)
struct FPlayerDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseMovementSpeed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseHealth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 baseHealthRegeneration;
};

USTRUCT(BlueprintType)
struct FCardDataTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 cardID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECardMonth_ex month;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsKwang;
};