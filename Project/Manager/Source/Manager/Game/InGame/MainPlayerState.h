// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "MainPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoldChangedNative, float NewGold);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChangedNative, float NewHP);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOwnedCardsChangedNative, const TArray<FOwnedCardInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerDataChangedNative, const FPlayerData&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAccumulatedUpgradesChangedNative, const FAccumulatedUpgrades&);

struct FMainPlayerReconnectSnapshot
{
	FWeaponData WeaponData;
	FPlayerData PlayerData;
	FCurPlayerData CurPlayerData;
	TArray<FOwnedCardInfo> OwnedCards;
	FOwnedCardInfo RevealedCard;
	FAccumulatedUpgrades AccumulatedUpgrades;
	TArray<int32> CarriedAmmoList;
	int32 LastRandomUpgradeClaimedRound = INDEX_NONE;
};

UCLASS()
class MANAGER_API AMainPlayerState : public APlayerState,
									 public IPhasePlayerStateInterface
{
	GENERATED_BODY()
	
public:
	static constexpr float BaseMaxHealth = 150.0f;

	AMainPlayerState();
private:
	float HoldingGold;
public:
	virtual int32 GetWeaponStatLV(EWeaponStatType StatType) const override;
	virtual int32 GetPlayerStatLV(EPlayerStatType StatType) const override;
	virtual int32 GetCurPlayerStatLV(ECurPlayerStatType StatType) const override;

	virtual EBulletType GetBulletID() const override;
	virtual EWeaponType GetWeaponID() const override;
	
	virtual void SetWeaponID(EWeaponType WeaponID) override;
	virtual void ResetState() override;
	void CaptureReconnectSnapshot(FMainPlayerReconnectSnapshot& OutSnapshot) const;
	void RestoreReconnectSnapshot(const FMainPlayerReconnectSnapshot& Snapshot);
	int32 LastRandomUpgradeClaimedRound = INDEX_NONE;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon Data")
	FWeaponData WeaponData;
	UPROPERTY(ReplicatedUsing = OnRep_PlayerData, BlueprintReadOnly, Category = "Player Data")
	FPlayerData PlayerData;

	UFUNCTION()
	void OnRep_PlayerData();

	UPROPERTY(ReplicatedUsing = OnRep_CurPlayerData, BlueprintReadOnly, Category = "Current Player Data")
	FCurPlayerData CurPlayerData;

	UPROPERTY(ReplicatedUsing = OnRep_OwnedCards, BlueprintReadOnly, Category = "Card Data")
	TArray<FOwnedCardInfo> OwnedCards;

	UPROPERTY(ReplicatedUsing = OnRep_PublicCardCount, BlueprintReadOnly, Category = "Card Data")
	int32 PublicCardCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RevealedCard, BlueprintReadOnly, Category = "Card Data")
	FOwnedCardInfo RevealedCard;

	UFUNCTION(BlueprintPure, Category = "Card")
	TArray<FOwnedCardInfo> GetOwnedCards() const;

	UFUNCTION(BlueprintPure, Category = "Card")
	FOwnedCardInfo GetRevealedCard() const { return RevealedCard; }

	UFUNCTION(BlueprintPure, Category = "Card")
	bool HasRevealedCard() const
	{
		return RevealedCard.CardInstanceId > 0 && RevealedCard.CardID != ECardID::None;
	}

	FString GetOwnedCardsDebugString() const;

	UFUNCTION(BlueprintPure, Category = "Card")
	bool HasOwnedCardInstance(int32 CardInstanceId) const;

	void AddOwnedCard(const FOwnedCardInfo& CardInfo);
	bool RemoveOwnedCardByInstanceId(int32 CardInstanceId, FOwnedCardInfo& OutRemovedCard);
	void ClearOwnedCards();
	void SetRevealedCard(const FOwnedCardInfo& CardInfo);
	void ClearRevealedCard();
	FOnOwnedCardsChangedNative OnOwnedCardsChangedNative;
	void AddGold(float Amount);

	FOnGoldChangedNative OnGoldChnageNative;

	void ApplyDamage(float ActualDamage);
	FOnHPChangedNative OnHPChnageNative;

	FOnPlayerDataChangedNative OnPlayerDataChangedNative;
	FOnAccumulatedUpgradesChangedNative OnAccumulatedUpgradesChangedNative;

	UPROPERTY(ReplicatedUsing = OnRep_AccumulatedUpgrades, BlueprintReadOnly, Category = "Player Upgrades")
	FAccumulatedUpgrades AccumulatedUpgrades;

	UFUNCTION()
	void OnRep_AccumulatedUpgrades();

protected:
	UFUNCTION()
	void OnRep_CurPlayerData(FCurPlayerData OldCurPlayerData);

	UFUNCTION()
	void OnRep_OwnedCards();

	UFUNCTION()
	void OnRep_PublicCardCount();

	UFUNCTION()
	void OnRep_RevealedCard();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player | Ammo")
	TArray<int32> CarriedAmmoList;
public:
	UFUNCTION()
	void Server_ApplyUpgrad_Implementation(EUpgradeType Type);

	void ApplyCardUpgrade(const TMap<EUpgradeType, float>& RolledStats);
	void ApplyWeaponUpgradePurchase(EUpgradeType Type);

	FAccumulatedUpgrades GetAccumulatedUpgrades() const { return AccumulatedUpgrades; };
public:
	float GetFinalMaxHP(float BaseMaxHP) const;
	float GetCurrentMaxHP() const { return GetFinalMaxHP(BaseMaxHealth); }
	float GetFinalRegenRate(float BaseRegen) const;
	float GetFinalMoveSpeed(float BaseMoveSpeed) const;
	float GetFinalMaxMagazine(float BaseMaxAmmo) const;
	float GetFinalReloadTimeMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Player | Ammo")
	int32 GetCarriedAmmoByWeaponType(EWeaponType WeaponType) const;
	UFUNCTION(BlueprintCallable, Category = "Player | Ammo")
	void AddCarriedAmmoByWeaponType(EWeaponType WeaponType, int32 Amount);
};
