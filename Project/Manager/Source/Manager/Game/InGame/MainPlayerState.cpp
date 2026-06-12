// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerState.h"
#include "Net/UnrealNetwork.h"

AMainPlayerState::AMainPlayerState()
{
    
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMainPlayerState, WeaponData);
    DOREPLIFETIME(AMainPlayerState, PlayerData);
}

int32 AMainPlayerState::GetWeaponStatLV(EWeaponStatType StatType) const
{
    switch (StatType)
    {
    case EWeaponStatType::None:                 return 0;
    case EWeaponStatType::Damage:               return WeaponData.LvDamage;
    case EWeaponStatType::FireRate:             return WeaponData.LvFireRate;
    case EWeaponStatType::Range:                return WeaponData.LvRange;
    case EWeaponStatType::MagazineCapacity:     return WeaponData.LvMagazineCapacity;
    case EWeaponStatType::ReloadTime:           return WeaponData.LvReloadTime;
    default:                                    return 0;
    }
}

int32 AMainPlayerState::GetPlayerStatLV(EPlayerStatType StatType) const
{
    switch (StatType)
    {
    case EPlayerStatType::None:                 return 0;
    case EPlayerStatType::MovementSpeed:        return PlayerData.LvMovementSpeed;
    case EPlayerStatType::Health:               return PlayerData.LvHealth;
    case EPlayerStatType::HealthRegeneration:   return PlayerData.LvHealthRegeneration;
    default:                                    return 0;
    }
}

EBulletType AMainPlayerState::GetBulletID() const
{
    return WeaponData.bulletID;
}

EWeaponType AMainPlayerState::GetWeaponID() const
{
    return WeaponData.weaponID;
}

void AMainPlayerState::SetWeaponID(EWeaponType WeaponID)
{
    WeaponData.weaponID = WeaponID;
    ForceNetUpdate();
}