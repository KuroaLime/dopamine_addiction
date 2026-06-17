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
    CurPlayerData.CurrentHP = FMath::Max(0, CurPlayerData.CurrentHP - static_cast<int32>(ActualDamage));
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
