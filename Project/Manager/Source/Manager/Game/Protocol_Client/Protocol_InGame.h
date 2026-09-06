// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Protocol_InGame.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None    = 0,
    AR      = 1,
    PISTOL      = 2,
    SHOTGUN     = 3,
    SMG      = 4,
    SNIPER      = 5

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
    None = 0,

    Jan_Gwang = 1,
    Jan_HongDdi = 2,

    Feb_Yul = 3,
    Feb_HongDdi = 4,

    Mar_Gwang = 5,
    Mar_HongDdi = 6,

    Apr_Yul = 7,
    Apr_ChoDdi = 8,

    May_Yul = 9,
    May_ChoDdi = 10,

    Jun_Yul = 11,
    Jun_CheongDdi = 12,

    Jul_Yul = 13,
    Jul_ChoDdi = 14,

    Aug_Gwang = 15,
    Aug_Yul = 16,

    Sep_Yul = 17,
    Sep_CheongDdi = 18,

    Oct_Yul = 19,
    Oct_CheongDdi = 20,
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

namespace CardDebug
{
    static FORCEINLINE FString ToString(ECardID CardID)
{
    switch (CardID)
    {
    case ECardID::Jan_Gwang:      return TEXT("Jan_Gwang");
    case ECardID::Jan_HongDdi:    return TEXT("Jan_HongDdi");

    case ECardID::Feb_Yul:        return TEXT("Feb_Yul");
    case ECardID::Feb_HongDdi:    return TEXT("Feb_HongDdi");

    case ECardID::Mar_Gwang:      return TEXT("Mar_Gwang");
    case ECardID::Mar_HongDdi:    return TEXT("Mar_HongDdi");

    case ECardID::Apr_Yul:        return TEXT("Apr_Yul");
    case ECardID::Apr_ChoDdi:     return TEXT("Apr_ChoDdi");

    case ECardID::May_Yul:        return TEXT("May_Yul");
    case ECardID::May_ChoDdi:     return TEXT("May_ChoDdi");

    case ECardID::Jun_Yul:        return TEXT("Jun_Yul");
    case ECardID::Jun_CheongDdi:  return TEXT("Jun_CheongDdi");

    case ECardID::Jul_Yul:        return TEXT("Jul_Yul");
    case ECardID::Jul_ChoDdi:     return TEXT("Jul_ChoDdi");

    case ECardID::Aug_Gwang:      return TEXT("Aug_Gwang");
    case ECardID::Aug_Yul:        return TEXT("Aug_Yul");

    case ECardID::Sep_Yul:        return TEXT("Sep_Yul");
    case ECardID::Sep_CheongDdi:  return TEXT("Sep_CheongDdi");

    case ECardID::Oct_Yul:        return TEXT("Oct_Yul");
    case ECardID::Oct_CheongDdi:  return TEXT("Oct_CheongDdi");

    default:                      return TEXT("None");
    }
}
}
inline constexpr int32 MaxUpgradeLevel = 5;

UENUM(BlueprintType)
enum class EUpgradeType : uint8 {
    None,
    Player_Health,
    Player_MoveSpeed,
    Player_HealthRegeneration,

    Weapon_Damage,
    Weapon_FireRate,
    Weapon_Range,
    Weapon_Magazine,
    Weapon_Reload
};

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

USTRUCT(BlueprintType)
struct FAccumulatedUpgrades
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly)
    float LvHealth = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvMoveSpeed = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvHealthRegen = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvWeaponDamage = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvWeaponFireRate = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvWeaponRange = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvWeaponMagazine = 0.f;
    UPROPERTY(BlueprintReadOnly)
    float LvWeaponReload = 0.f;
};
//////////////////////////////////////////////////////////

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
struct FStatRangeInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MinValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxValue = 0.0f;
};
USTRUCT(BlueprintType)
struct FRandomUpgradeCardDataTable : public FTableRowBase
{
    GENERATED_BODY()

public:
    FRandomUpgradeCardDataTable() : CardTitle(FText::GetEmpty()) , CardTexture(nullptr), CardDescription(FText::GetEmpty())
    { }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText CardTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    TObjectPtr<UTexture2D> CardTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText CardDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    TMap<EUpgradeType, FStatRangeInfo> UpgradeValue;
};
USTRUCT(BlueprintType)
struct FRandomCardOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName CardRowName;

    UPROPERTY(BlueprintReadOnly)
    TMap<EUpgradeType, float> RolledStats;
};
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

    // 탄퍼짐 콘 반각(도). 0이면 무탄퍼짐(저격 등), 샷건은 크게.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadAngle = 0.f;
    // 한 발에 발사되는 펠릿 수. 샷건 외에는 1.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PelletCount = 1;
    // 히트 판정 스윕 반지름(cm). 0이면 기존과 동일한 두께 0 라인 트레이스.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TraceRadius = 0.f;
    // true면 눌러서 홀드 중 FireRate 주기로 계속 발사(연사). false면 클릭당 1발만(세미오토).
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bFullAuto = true;

    // 연사(같은 트리거 홀드) 중 BloomStartShotCount발을 넘긴 다음부터 한 발마다 SpreadAngle에 추가로
    // 누적되는 각도(도). 실제 탄 판정 원뿔이 벌어짐.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BloomPerShot = 0.f;
    // 블룸으로 늘어날 수 있는 최대 추가 각도(도). 손을 떼면(다음 트리거 홀드 시작 시) 0으로 리셋.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxBloomAngle = 0.f;
    // 이 발수까지는 블룸이 붙지 않고 SpreadAngle 그대로 나간다(예: 3~5).
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BloomStartShotCount = 0;
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
    Discarded = 5,
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
