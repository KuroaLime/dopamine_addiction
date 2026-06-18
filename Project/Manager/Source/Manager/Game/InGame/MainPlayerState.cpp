// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerState.h"
#include "Net/UnrealNetwork.h"

AMainPlayerState::AMainPlayerState()
{
    CurPlayerData.HoldingGold = 10;
    CurPlayerData.CurrentHP = 150;
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMainPlayerState, WeaponData);
    DOREPLIFETIME(AMainPlayerState, PlayerData);
    DOREPLIFETIME(AMainPlayerState, CurPlayerData);

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

int32 AMainPlayerState::GetCurPlayerStatLV(ECurPlayerStatType StatType) const
{
    switch (StatType)
    {
    case ECurPlayerStatType::None:              return 0;
    case ECurPlayerStatType::CurrentHP:         return CurPlayerData.CurrentHP;
    case ECurPlayerStatType::HoldingGold:       return CurPlayerData.HoldingGold;
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

void AMainPlayerState::AddGold(float Amount)
{
    if (!HasAuthority())
        return;
    CurPlayerData.HoldingGold += Amount;
    ForceNetUpdate();
    //OnRep_CurPlayerData(ECurPlayerStatType::HoldingGold);
}

void AMainPlayerState::ApplyDamage(float ActualDamage)
{
    if (!HasAuthority())
        return;
    CurPlayerData.CurrentHP -= ActualDamage;
    //OnRep_CurPlayerData();
    ForceNetUpdate();
}

void AMainPlayerState::OnRep_CurPlayerData(FCurPlayerData OldCurPlayerData)
{
    if (OldCurPlayerData.HoldingGold != CurPlayerData.HoldingGold) {
        OnGoldChnageNative.Broadcast(CurPlayerData.HoldingGold);
    }
    if (OldCurPlayerData.CurrentHP != CurPlayerData.CurrentHP) {
        OnHPChnageNative.Broadcast(CurPlayerData.CurrentHP);
    }
    
}

void AMainPlayerState::Server_ApplyUpgrad_Implementation(EUpgradeType Type)
{
    //업그레이드 골드가 있는지 체크 ㄱㄱ
    if (CurPlayerData.HoldingGold) {

    }
    switch (Type) {
    case EUpgradeType::Player_Health:
        if (PlayerData.LvHealth < Max_UpgradeLevel) {
            PlayerData.LvHealth++;
        }
        break;
    case EUpgradeType::Player_MoveSpeed:
        if (PlayerData.LvMovementSpeed < Max_UpgradeLevel) {
            PlayerData.LvMovementSpeed++;
        }
        break;
    case EUpgradeType::Player_HealthRegeneration:
        if (PlayerData.LvHealthRegeneration < Max_UpgradeLevel) {
            PlayerData.LvHealthRegeneration++;
        }
        break;
    case EUpgradeType::Weapon_Damage:
        if (WeaponData.LvDamage < Max_UpgradeLevel) {
            WeaponData.LvDamage++;
        }
        break;
    case EUpgradeType::Weapon_FireRate:
        if (WeaponData.LvFireRate < Max_UpgradeLevel) {
            WeaponData.LvFireRate++;
        }
        break;
    case EUpgradeType::Weapon_Range:
        if (WeaponData.LvRange < Max_UpgradeLevel) {
            WeaponData.LvRange++;
        }
        break;
    case EUpgradeType::Weapon_Magazine:
        if (WeaponData.LvMagazineCapacity < Max_UpgradeLevel) {
            WeaponData.LvMagazineCapacity++;
        }
        break;
    case EUpgradeType::Weapon_Reload:
        if (WeaponData.LvReloadTime < Max_UpgradeLevel) {
            WeaponData.LvReloadTime++;
        }
        break;
    default:
        break;
    }


    //변경되었음을 알리고 적용하도록 하는 함수
}