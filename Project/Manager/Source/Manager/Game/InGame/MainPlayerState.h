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

UCLASS()
class MANAGER_API AMainPlayerState : public APlayerState,
									 public IPhasePlayerStateInterface
{
	GENERATED_BODY()
	
public:
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

	UFUNCTION(BlueprintPure, Category = "Card")
	TArray<FOwnedCardInfo> GetOwnedCards() const;

	FString GetOwnedCardsDebugString() const;

	UFUNCTION(BlueprintPure, Category = "Card")
	bool HasOwnedCardInstance(int32 CardInstanceId) const;

	void AddOwnedCard(const FOwnedCardInfo& CardInfo);
	bool RemoveOwnedCardByInstanceId(int32 CardInstanceId, FOwnedCardInfo& OutRemovedCard);
	void ClearOwnedCards();
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
public:
	UFUNCTION()
	void Server_ApplyUpgrad_Implementation(EUpgradeType Type);

	void ApplyCardUpgrade(const TMap<EUpgradeType, float>& RolledStats);

	FAccumulatedUpgrades GetAccumulatedUpgrades() const { return AccumulatedUpgrades; };
public:
	float GetFinalMaxHP(float BaseMaxHP) const;
	float GetFinalRegenRate(float BaseRegen) const;
	float GetFinalMoveSpeed(float BaseMoveSpeed) const;
	float GetFinalWeaponDamageMultiplier() const;
	float GetFinalFireDelayMultiplier() const;
	float GetFinalWeaponRangeMultiplier() const;
	float GetFinalMaxMagazine(float BaseMaxAmmo) const;
	float GetFinalReloadTimeMultiplier() const;
};
