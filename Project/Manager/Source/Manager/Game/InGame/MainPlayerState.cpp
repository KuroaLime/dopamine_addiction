// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

AMainPlayerState::AMainPlayerState()
{
	CurPlayerData.HoldingGold = 10000;
	CurPlayerData.CurrentHP = 150;


	//하드 코딩 바꾸자
	CarriedAmmoList.Init(0, 6);
	CarriedAmmoList[static_cast<uint8>(EWeaponType::AR)] = 90;
	CarriedAmmoList[static_cast<uint8>(EWeaponType::PISTOL)] = 30;
	CarriedAmmoList[static_cast<uint8>(EWeaponType::SHOTGUN)] = 15;
	CarriedAmmoList[static_cast<uint8>(EWeaponType::SMG)] = 120;
	CarriedAmmoList[static_cast<uint8>(EWeaponType::SNIPER)] = 10;
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainPlayerState, WeaponData);
	DOREPLIFETIME(AMainPlayerState, PlayerData);
	DOREPLIFETIME(AMainPlayerState, CurPlayerData);
	DOREPLIFETIME_CONDITION(AMainPlayerState, OwnedCards, COND_OwnerOnly);
	DOREPLIFETIME(AMainPlayerState, PublicCardCount);
	DOREPLIFETIME(AMainPlayerState, AccumulatedUpgrades);
	DOREPLIFETIME(AMainPlayerState, CarriedAmmoList);
}

int32 AMainPlayerState::GetWeaponStatLV(EWeaponStatType StatType) const
{
	switch (StatType)
	{
	case EWeaponStatType::None:
		return 0;
	case EWeaponStatType::Damage:
		return WeaponData.LvDamage + FMath::RoundToInt(AccumulatedUpgrades.LvWeaponDamage);
	case EWeaponStatType::FireRate:
		return WeaponData.LvFireRate + FMath::RoundToInt(AccumulatedUpgrades.LvWeaponFireRate);
	case EWeaponStatType::Range:
		return WeaponData.LvRange + FMath::RoundToInt(AccumulatedUpgrades.LvWeaponRange);
	case EWeaponStatType::MagazineCapacity:
		return WeaponData.LvMagazineCapacity + FMath::RoundToInt(AccumulatedUpgrades.LvWeaponMagazine);
	case EWeaponStatType::ReloadTime:
		return WeaponData.LvReloadTime + FMath::RoundToInt(AccumulatedUpgrades.LvWeaponReload);
	default:
		return 0;
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

FString AMainPlayerState::GetOwnedCardsDebugString() const
{
    TArray<FString> Parts;

    for (const FOwnedCardInfo& CardInfo : OwnedCards)
    {
        Parts.Add(FString::Printf(
            TEXT("#%d:%s"),
            CardInfo.CardInstanceId,
            *CardDebug::ToString(CardInfo.CardID)
        ));
    }

    return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("Empty");
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

    const FString CardList = GetOwnedCardsDebugString();

    UE_LOG(LogTemp, Warning, TEXT("[CL] OwnedCardsChanged Player=%s Count=%d Cards=[%s]"),
        *GetPlayerName(),
        OwnedCards.Num(),
        *CardList);

    if (GEngine)
    {
        constexpr int32 MyCardsDebugMessageKey = 20260623;

        const FString Message = FString::Printf(
            TEXT("[MY CARDS %d/3]\n%s"),
            OwnedCards.Num(),
            *CardList
        );

        GEngine->AddOnScreenDebugMessage(
            MyCardsDebugMessageKey,
            9999.0f,
            FColor::Cyan,
            Message
        );
    }
}

void AMainPlayerState::OnRep_PublicCardCount()
{
}

void AMainPlayerState::OnRep_PlayerData()
{
	OnPlayerDataChangedNative.Broadcast(PlayerData);
}

void AMainPlayerState::OnRep_AccumulatedUpgrades()
{
	OnAccumulatedUpgradesChangedNative.Broadcast(AccumulatedUpgrades);
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
			CurPlayerData.CurrentHP += 1;
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
	default:
		break;
	}
	if (OnPlayerDataChangedNative.IsBound())
	{
		OnPlayerDataChangedNative.Broadcast(PlayerData);
	}
	ForceNetUpdate();
}

void AMainPlayerState::ApplyCardUpgrade(const TMap<EUpgradeType, float>& RolledStats)
{
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("ApplyCardUpgrade")));

	if (!HasAuthority()) return;
	for (const auto& Pair : RolledStats)
	{
		EUpgradeType UpgradeType = Pair.Key;
		float RolledValue = Pair.Value;
		switch (UpgradeType)
		{
		case EUpgradeType::Player_Health:
			AccumulatedUpgrades.LvHealth += RolledValue;
			CurPlayerData.CurrentHP += FMath::RoundToInt(RolledValue);
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Success")));

			break;
		case EUpgradeType::Player_MoveSpeed:
			AccumulatedUpgrades.LvMoveSpeed += RolledValue;
			break;
		case EUpgradeType::Player_HealthRegeneration:
			AccumulatedUpgrades.LvHealthRegen += RolledValue;
			break;
		case EUpgradeType::Weapon_Damage:
			AccumulatedUpgrades.LvWeaponDamage += RolledValue;
			break;
		case EUpgradeType::Weapon_FireRate:
			AccumulatedUpgrades.LvWeaponFireRate += RolledValue;
			break;
		case EUpgradeType::Weapon_Range:
			AccumulatedUpgrades.LvWeaponRange += RolledValue;
			break;
		case EUpgradeType::Weapon_Magazine:
			AccumulatedUpgrades.LvWeaponMagazine += RolledValue;
			break;
		case EUpgradeType::Weapon_Reload:
			AccumulatedUpgrades.LvWeaponReload += RolledValue;
			break;
		default:
			break;
		}
	}
	if (OnAccumulatedUpgradesChangedNative.IsBound())
	{
		OnAccumulatedUpgradesChangedNative.Broadcast(AccumulatedUpgrades);
	}
	ForceNetUpdate();
}


float AMainPlayerState::GetFinalMaxHP(float BaseMaxHP) const {
	return BaseMaxHP + (PlayerData.LvHealth * 20.0f) + AccumulatedUpgrades.LvHealth;
}
float AMainPlayerState::GetFinalRegenRate(float BaseRegen) const {
	return BaseRegen + (PlayerData.LvHealthRegeneration * 1.0f) + AccumulatedUpgrades.LvHealthRegen;
}
float AMainPlayerState::GetFinalMoveSpeed(float BaseMoveSpeed) const {
	return BaseMoveSpeed + (PlayerData.LvMovementSpeed * 100.0f) + AccumulatedUpgrades.LvMoveSpeed;
}
float AMainPlayerState::GetFinalWeaponDamageMultiplier() const {
	return 1.0f + (WeaponData.LvDamage * 0.15f) + AccumulatedUpgrades.LvWeaponDamage;
}
float AMainPlayerState::GetFinalFireDelayMultiplier() const {
	return 1.0f + (WeaponData.LvFireRate * 0.10f) + AccumulatedUpgrades.LvWeaponFireRate;
}
float AMainPlayerState::GetFinalWeaponRangeMultiplier() const {
	return 1.0f + (WeaponData.LvRange * 0.15f) + AccumulatedUpgrades.LvWeaponRange;
}
float AMainPlayerState::GetFinalMaxMagazine(float BaseMaxAmmo) const {
	return BaseMaxAmmo + (WeaponData.LvMagazineCapacity * 4.0f) + AccumulatedUpgrades.LvWeaponMagazine;
}
float AMainPlayerState::GetFinalReloadTimeMultiplier() const {
	return 1.0f + (WeaponData.LvReloadTime * 0.12f) + AccumulatedUpgrades.LvWeaponReload;
}

int32 AMainPlayerState::GetCarriedAmmoByWeaponType(EWeaponType WeaponType) const
{
	uint8 Index = static_cast<uint8>(WeaponType);
	if (CarriedAmmoList.IsValidIndex(Index))
	{
		return CarriedAmmoList[Index];
	}
	return 0;
}
void AMainPlayerState::AddCarriedAmmoByWeaponType(EWeaponType WeaponType, int32 Amount)
{
	if (!HasAuthority()) return;
	uint8 Index = static_cast<uint8>(WeaponType);
	if (CarriedAmmoList.IsValidIndex(Index))
	{
		CarriedAmmoList[Index] = FMath::Max(0, CarriedAmmoList[Index] + Amount);
		ForceNetUpdate();
	}
}