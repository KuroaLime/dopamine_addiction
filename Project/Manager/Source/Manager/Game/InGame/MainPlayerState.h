// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "MainPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoldChangedNative, float NewGold);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChangedNative, float NewHP);

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
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Data")
	FPlayerData PlayerData;

	UPROPERTY(ReplicatedUsing = OnRep_CurPlayerData, BlueprintReadOnly, Category = "Current Player Data")
	FCurPlayerData CurPlayerData;


	void AddGold(float Amount);
	FOnGoldChangedNative OnGoldChnageNative;

	void ApplyDamage(float ActualDamage);
	FOnHPChangedNative OnHPChnageNative;

protected:
	UFUNCTION()
	void OnRep_CurPlayerData(FCurPlayerData OldCurPlayerData);
public:
	UFUNCTION()
	void Server_ApplyUpgrad_Implementation(EUpgradeType Type);
};
