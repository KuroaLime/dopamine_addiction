// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainGameState.h"
#include "UObject/ConstructorHelpers.h"
//#include "Net/UnrealNetwork.h"

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

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan,
				FString::Printf(TEXT("Load Card Count: %d"), CardDataMap.Num()));

			// TMap을 순회하는 가장 안전한 Iterator 방식
			for (auto It = CardDataMap.CreateConstIterator(); It; ++It)
			{
				// It.Key(), It.Value() 함수를 통해 데이터를 꺼냅니다.
				ECardID CardID = It.Key();
				const FCardDataTable& CardData = It.Value();

				int32 ID_Num = static_cast<int32>(CardID);
				int32 Month_Num = static_cast<int32>(CardData.cardMonth);

				FString TypeName = UEnum::GetValueAsString(CardData.cardType);
				TypeName.Split(TEXT("::"), nullptr, &TypeName);

				FString DebugMsg = FString::Printf(TEXT("ID: %02d | %2d Month | Type: %s"),
					ID_Num, Month_Num, *TypeName);

				GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, DebugMsg);
				UE_LOG(LogTemp, Log, TEXT("[CardData] %s"), *DebugMsg);
			}
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
}