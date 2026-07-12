// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "MainGameState.generated.h"

/**
 * 
 */



UCLASS()
class MANAGER_API AMainGameState : public AGameStateBase,
								   public IPhaseGameStateInterface
{
	GENERATED_BODY()
	
public:
	AMainGameState();

	UPROPERTY(BlueprintReadOnly, Category = "Master Data | Weapon")
	TMap<EBulletType, FBulletDataTable> BulletDataMap;

	UPROPERTY(BlueprintReadOnly, Category = "Master Data | Weapon")
	TMap<EWeaponType, FWeaponDataTable> WeaponDataMap;

	UPROPERTY(BlueprintReadOnly, Category = "Master Data | Card")
	TMap<ECardID, FCardDataTable> CardDataMap;

	UPROPERTY(BlueprintReadOnly, Category = "Master Data | Player")
	FPlayerDataTable BasePlayerData;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTimeUpdated OnTimeUpdated;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round Data")
	EWeaponType CurrentRoundWeapon;

	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime)
	int32 RemainingTime;

	UPROPERTY(BlueprintReadOnly, Category = "Master Data | Random3Card")
	TArray<FRandomUpgradeCardDataTable> RandCardData;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual int32 GetRemainingTime() const override { return RemainingTime; }
	virtual FOnTimeUpdated& GetOnTimeUpdated() override { return OnTimeUpdated; }
	virtual void SetRemainingTime(int32 NewTime) override { RemainingTime = NewTime; }
	virtual void BroadcastTimeUpdated(int32 NewTime) override
	{
		if (OnTimeUpdated.IsBound())
			OnTimeUpdated.Broadcast(NewTime);
	}

	virtual void SetRoundWeapon(EWeaponType InWeaponID) override;
	virtual EWeaponType GetWeaponID() const override;
	virtual int32 GetWeaponBaseData(EWeaponType WeaponID, EWeaponBaseStatType StatType) const override;
	virtual void GetWeaponFireProfile(EWeaponType WeaponID, float& OutSpreadAngle, int32& OutPelletCount, float& OutTraceRadius) const override;
	virtual void GetWeaponFireMode(EWeaponType WeaponID, bool& OutFullAuto) const override;
	virtual void GetWeaponRecoil(EWeaponType WeaponID, float& OutRecoilPitch, float& OutRecoilYaw) const override;
	class UDataTable* GetShopRandomCardDataTable() const { return ShopRandomCardDataTableAsset; }
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_RemainingTime();
private:
	UPROPERTY(EditDefaultsOnly, Category = "Master Data | Setup", meta = (AllowPrivateAccess = "true"))
	class UDataTable* BulletDataTableAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Master Data | Setup", meta = (AllowPrivateAccess = "true"))
	class UDataTable* WeaponDataTableAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Master Data | Setup", meta = (AllowPrivateAccess = "true"))
	class UDataTable* CardDataTableAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Master Data | Setup", meta = (AllowPrivateAccess = "true"))
	class UDataTable* PlayerDataTableAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Master Data | Setup", meta = (AllowPrivateAccess = "true"))
	class UDataTable* ShopRandomCardDataTableAsset;

	void InitializeMasterData();
public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRoundChanged OnRoundChanged;
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "Round Data")
	int32 CurrentRound = 1;
	UFUNCTION()
	void OnRep_CurrentRound();

	virtual FOnRoundChanged& GetOnRoundChanged() override { return OnRoundChanged; }
};
