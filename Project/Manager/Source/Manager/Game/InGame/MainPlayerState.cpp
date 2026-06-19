// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
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
	DOREPLIFETIME_CONDITION(AMainPlayerState, OwnedCards, COND_OwnerOnly);
	DOREPLIFETIME(AMainPlayerState, PublicCardCount);
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

TArray<FOwnedCardInfo> AMainPlayerState::GetOwnedCards() const
{
	return OwnedCards;
}

bool AMainPlayerState::HasOwnedCardInstance(int32 CardInstanceId) const
{
	return OwnedCards.ContainsByPredicate([CardInstanceId](const FOwnedCardInfo& CardInfo)
	{
		return CardInfo.CardInstanceId == CardInstanceId;
	});
}

void AMainPlayerState::AddOwnedCard(const FOwnedCardInfo& CardInfo)
{
	if (!HasAuthority() || CardInfo.CardInstanceId <= 0 || CardInfo.CardID == ECardID::None)
	{
		return;
	}

	if (HasOwnedCardInstance(CardInfo.CardInstanceId))
	{
		return;
	}

	OwnedCards.Add(CardInfo);
	PublicCardCount = OwnedCards.Num();
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
}

bool AMainPlayerState::RemoveOwnedCardByInstanceId(int32 CardInstanceId, FOwnedCardInfo& OutRemovedCard)
{
	if (!HasAuthority())
	{
		return false;
	}

	const int32 Index = OwnedCards.IndexOfByPredicate([CardInstanceId](const FOwnedCardInfo& CardInfo)
	{
		return CardInfo.CardInstanceId == CardInstanceId;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutRemovedCard = OwnedCards[Index];
	OwnedCards.RemoveAt(Index);
	PublicCardCount = OwnedCards.Num();
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
	return true;
}

void AMainPlayerState::ClearOwnedCards()
{
	if (!HasAuthority())
	{
		return;
	}

	OwnedCards.Empty();
	PublicCardCount = 0;
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
}

void AMainPlayerState::ResetState()
{
	if (!HasAuthority())
	{
		return;
	}

	CurPlayerData.CurrentHP = 150;
	ForceNetUpdate();
}

// 아직 리셋시킬 데이터에 대해 확정된 상태가 아니라 일단 HP만 리셋
void AMainPlayerState::ResetState()
{
    if (!HasAuthority())
        return;
    CurPlayerData.CurrentHP = 150;
}

void AMainPlayerState::AddGold(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}

	CurPlayerData.HoldingGold += Amount;
	ForceNetUpdate();
}

void AMainPlayerState::ApplyDamage(float ActualDamage)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 OldHP = CurPlayerData.CurrentHP;
	CurPlayerData.CurrentHP = FMath::Max(0, OldHP - static_cast<int32>(ActualDamage));

	UE_LOG(LogTemp, Warning, TEXT("[DS] TPS DamageApplied Player=%s Damage=%.2f HP=%d->%d"),
		*GetPlayerName(),
		ActualDamage,
		OldHP,
		CurPlayerData.CurrentHP);

	ForceNetUpdate();
}

void AMainPlayerState::OnRep_CurPlayerData(FCurPlayerData OldCurPlayerData)
{
	if (OldCurPlayerData.HoldingGold != CurPlayerData.HoldingGold)
	{
		OnGoldChnageNative.Broadcast(CurPlayerData.HoldingGold);
	}
	if (OldCurPlayerData.CurrentHP != CurPlayerData.CurrentHP)
	{
		OnHPChnageNative.Broadcast(CurPlayerData.CurrentHP);
	}
}

void AMainPlayerState::OnRep_OwnedCards()
{
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
}

void AMainPlayerState::OnRep_PublicCardCount()
{
}

void AMainPlayerState::Server_ApplyUpgrad_Implementation(EUpgradeType Type)
{
	if (!HasAuthority())
	{
		return;
	}

	constexpr int32 MaxUpgradeLevel = 5;

	switch (Type)
	{
	case EUpgradeType::Player_Health:
		if (PlayerData.LvHealth < MaxUpgradeLevel)
		{
			PlayerData.LvHealth++;
		}
		break;
	case EUpgradeType::Player_MoveSpeed:
		if (PlayerData.LvMovementSpeed < MaxUpgradeLevel)
		{
			PlayerData.LvMovementSpeed++;
		}
		break;
	case EUpgradeType::Player_HealthRegeneration:
		if (PlayerData.LvHealthRegeneration < MaxUpgradeLevel)
		{
			PlayerData.LvHealthRegeneration++;
		}
		break;
	case EUpgradeType::Weapon_Damage:
		if (WeaponData.LvDamage < MaxUpgradeLevel)
		{
			WeaponData.LvDamage++;
		}
		break;
	case EUpgradeType::Weapon_FireRate:
		if (WeaponData.LvFireRate < MaxUpgradeLevel)
		{
			WeaponData.LvFireRate++;
		}
		break;
	case EUpgradeType::Weapon_Range:
		if (WeaponData.LvRange < MaxUpgradeLevel)
		{
			WeaponData.LvRange++;
		}
		break;
	case EUpgradeType::Weapon_Magazine:
		if (WeaponData.LvMagazineCapacity < MaxUpgradeLevel)
		{
			WeaponData.LvMagazineCapacity++;
		}
		break;
	case EUpgradeType::Weapon_Reload:
		if (WeaponData.LvReloadTime < MaxUpgradeLevel)
		{
			WeaponData.LvReloadTime++;
		}
		break;
	default:
		break;
	}

	ForceNetUpdate();
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