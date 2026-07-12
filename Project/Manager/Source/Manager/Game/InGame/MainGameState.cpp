// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainGameState.h"
#include "UObject/ConstructorHelpers.h"

AMainGameState::AMainGameState()
{
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UDataTable> BulletDataTableFinder(TEXT("/Game/GameData/DT_Bullet.DT_Bullet"));
	if (BulletDataTableFinder.Succeeded()) BulletDataTableAsset = BulletDataTableFinder.Object;

	static ConstructorHelpers::FObjectFinder<UDataTable> WeaponDataTableFinder(TEXT("/Game/GameData/DT_Weapon.DT_Weapon"));
	if (WeaponDataTableFinder.Succeeded()) WeaponDataTableAsset = WeaponDataTableFinder.Object;

	static ConstructorHelpers::FObjectFinder<UDataTable> CardDataTableFinder(TEXT("/Game/GameData/DT_Card.DT_Card"));
	if (CardDataTableFinder.Succeeded()) CardDataTableAsset = CardDataTableFinder.Object;

	static ConstructorHelpers::FObjectFinder<UDataTable> PlayerDataTableFinder(TEXT("/Game/GameData/DT_Player.DT_Player"));
	if (PlayerDataTableFinder.Succeeded()) PlayerDataTableAsset = PlayerDataTableFinder.Object;

	static ConstructorHelpers::FObjectFinder<UDataTable> ShopRandomCardDataTableAssetFinder(TEXT("/Game/GameData/DT_UpgradeCardInfo.DT_UpgradeCardInfo"));
	if (ShopRandomCardDataTableAssetFinder.Succeeded()) ShopRandomCardDataTableAsset = ShopRandomCardDataTableAssetFinder.Object;
}

void AMainGameState::BeginPlay()
{
	Super::BeginPlay();

	InitializeMasterData();
}

void AMainGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainGameState, RemainingTime);
	DOREPLIFETIME(AMainGameState, CurrentRoundWeapon);
	DOREPLIFETIME(AMainGameState, CurrentRound);
}
void AMainGameState::OnRep_CurrentRound()
{
	if (OnRoundChanged.IsBound())
	{
		OnRoundChanged.Broadcast(CurrentRound);
	}
}
void AMainGameState::SetRoundWeapon(EWeaponType InWeaponID)
{
	if (HasAuthority())
	{
		CurrentRoundWeapon = InWeaponID;
	}
}

EWeaponType AMainGameState::GetWeaponID() const
{
	return CurrentRoundWeapon;
}

int32 AMainGameState::GetWeaponBaseData(EWeaponType WeaponID, EWeaponBaseStatType StatType) const
{
	if (const FWeaponDataTable* FoundData = WeaponDataMap.Find(WeaponID))
	{
		switch (StatType)
		{
		case EWeaponBaseStatType::None:             return 0;
		case EWeaponBaseStatType::Damage:           return FoundData->BaseDamage;
		case EWeaponBaseStatType::FireRate:         return FoundData->BaseFireRate;
		case EWeaponBaseStatType::Range:            return FoundData->BaseRange;
		case EWeaponBaseStatType::MagazineCapacity: return FoundData->BaseMagazineCapacity;
		case EWeaponBaseStatType::ReloadTime:       return FoundData->BaseReloadTime;
		default:                                    return 0;
		}
	}
	return 0;
}

void AMainGameState::GetWeaponFireProfile(EWeaponType WeaponID, float& OutSpreadAngle, int32& OutPelletCount, float& OutTraceRadius) const
{
	OutSpreadAngle = 0.f;
	OutPelletCount = 1;
	OutTraceRadius = 0.f;

	if (const FWeaponDataTable* FoundData = WeaponDataMap.Find(WeaponID))
	{
		OutSpreadAngle = FoundData->SpreadAngle;
		OutPelletCount = FMath::Max(1, FoundData->PelletCount);
		OutTraceRadius = FoundData->TraceRadius;
	}
}

void AMainGameState::GetWeaponFireMode(EWeaponType WeaponID, bool& OutFullAuto) const
{
	OutFullAuto = true;

	if (const FWeaponDataTable* FoundData = WeaponDataMap.Find(WeaponID))
	{
		OutFullAuto = FoundData->bFullAuto;
	}
}

void AMainGameState::GetWeaponRecoil(EWeaponType WeaponID, float& OutRecoilPitch, float& OutRecoilYaw) const
{
	OutRecoilPitch = 0.f;
	OutRecoilYaw = 0.f;

	if (const FWeaponDataTable* FoundData = WeaponDataMap.Find(WeaponID))
	{
		OutRecoilPitch = FoundData->RecoilPitch;
		OutRecoilYaw = FoundData->RecoilYaw;
	}
}

void AMainGameState::OnRep_RemainingTime() {
	if (OnTimeUpdated.IsBound())
		OnTimeUpdated.Broadcast(RemainingTime);
}

void AMainGameState::InitializeMasterData()
{
	if (BulletDataTableAsset)
	{
		TArray<FBulletDataTable*> Rows;
		BulletDataTableAsset->GetAllRows<FBulletDataTable>(TEXT("Context_Bullet"), Rows);
		for (FBulletDataTable* Row : Rows)
		{
			if (Row) BulletDataMap.Add(Row->bulletID, *Row);
		}
	}

	if (WeaponDataTableAsset)
	{
		TArray<FWeaponDataTable*> Rows;
		WeaponDataTableAsset->GetAllRows<FWeaponDataTable>(TEXT("Context_Weapon"), Rows);
		for (FWeaponDataTable* Row : Rows)
		{
			if (Row) WeaponDataMap.Add(Row->weaponID, *Row);
		}
	}

	if (CardDataTableAsset)
	{
		TArray<FCardDataTable*> Rows;
		CardDataTableAsset->GetAllRows<FCardDataTable>(TEXT("Context_Card"), Rows);
		for (FCardDataTable* Row : Rows)
		{
			if (Row) CardDataMap.Add(Row->cardID, *Row);
		}
	}

	if (PlayerDataTableAsset)
	{
		TArray<FPlayerDataTable*> Rows;
		PlayerDataTableAsset->GetAllRows<FPlayerDataTable>(TEXT("Context_Player"), Rows);
		if (Rows.Num() > 0 && Rows[0])
		{
			BasePlayerData = *Rows[0];
		}
	}

	if (ShopRandomCardDataTableAsset)
	{
		TArray<FRandomUpgradeCardDataTable*> Rows;
		ShopRandomCardDataTableAsset->GetAllRows<FRandomUpgradeCardDataTable>(TEXT("Context_Player"), Rows);
		for (FRandomUpgradeCardDataTable* Row : Rows)
		{
			if (Row) RandCardData.Add(*Row);
		}
	}
}