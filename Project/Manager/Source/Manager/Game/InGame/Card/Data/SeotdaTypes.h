// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SeotdaTypes.generated.h"

//////////////////////////////////////////////////////////////////////////////////////
// 1. ������

UENUM(BlueprintType)
enum class EAIStyle : uint8
{
	Balanced,
	Conservative,
	Aggressive,
	Gambler
};

UENUM(BlueprintType)
enum class EBettingAction : uint8
{
    None,
    Check,
    Call,
    Quarter,
    Half,
    Ddadang,
    Pping,
    Die,
    AllIn
};

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	BattleRoyale UMETA(DisplayName = "Battle Royale"),
	SeotdaCardSelection UMETA(DisplayName = "Seotda Card Selection"),
	SeotdaBetting UMETA(DisplayName = "Seotda Betting"),
	SeotdaResult UMETA(DisplayName = "Seotda Result")
};

UENUM(BlueprintType)
enum class ECardMonth : uint8
{
	None = 0,
	Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5,
	Jun = 6, Jul = 7, Aug = 8, Sep = 9, Oct = 10
};

//////////////////////////////////////////////////////////////////////////////////////
// 2. ���� ��ŷ ���

namespace SeotdaRank {
	const int32 TRIPLE_38 = 3000;
	const int32 TRIPLE_13_18 = 2000;
	const int32 PAIR_BASE = 1000;
}

//////////////////////////////////////////////////////////////////////////////////////
// 3. ī�� ����

USTRUCT(BlueprintType)
struct FSeotdaCard
{
	GENERATED_BODY()

public:
	ECardMonth Month = ECardMonth::None;
	bool bIsKwang = false;

public:
	FSeotdaCard() {}
	FSeotdaCard(ECardMonth InMonth, bool bKwang) : Month(InMonth), bIsKwang(bKwang) {}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d%s"), (int32)Month, bIsKwang ? TEXT("KWANG") : TEXT(""));
	}

	static int32 GetScore(const FSeotdaCard& A, const FSeotdaCard& B)
	{
		int32 M1 = (int32)A.Month;
		int32 M2 = (int32)B.Month;

		if (A.bIsKwang && B.bIsKwang)
		{
			if ((M1 == 3 && M2 == 8) || (M1 == 8 && M2 == 3)) return SeotdaRank::TRIPLE_38;
			return SeotdaRank::TRIPLE_13_18;
		}

		if (M1 == M2) return SeotdaRank::PAIR_BASE + (M1 * 10);

		return (M1 + M2) % 10;
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// 4. �÷��̾� ����

USTRUCT(BlueprintType)
struct FSeotdaPlayerInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAIStyle Style = EAIStyle::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSeotdaCard> Hand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 BetMoney = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsFolded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAI = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasSelectedCards = false;

	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LastActionStatus = TEXT("-");

public:
	void ResetForNewRound()
	{
		// ��Ʋ�ξ⿡�� ȹ���� ī��� �����ؾ� �ϹǷ� Hand�� ����� �ʴ´�.
		Score = 0;
		bIsFolded = false;
		BetMoney = 0;
		bHasSelectedCards = false;
		LastActionStatus = TEXT("-");
	}

	bool CanAfford(int64 Amount) const { return Money >= Amount; }

	void Pay(int64 Amount, int64& GlobalPot) {
		int64 ActualAmount = FMath::Min(Amount, Money);
		Money -= ActualAmount;
		BetMoney += ActualAmount;
		GlobalPot += ActualAmount;
	}

	int64 CalculateRequiredPay(EBettingAction Action, int64 GlobalMaxBet, int64 GlobalPot) const
	{
		int64 CallDiff = GlobalMaxBet - BetMoney;
		switch (Action) {
		case EBettingAction::Check:
		case EBettingAction::Call:  return CallDiff;
		case EBettingAction::Quarter: return CallDiff + FMath::Max<int64>(1, (GlobalPot + CallDiff) / 4);
   		case EBettingAction::Half:  return CallDiff + FMath::Max<int64>(1, (GlobalPot + CallDiff) / 2);
   		case EBettingAction::Ddadang: return CallDiff + FMath::Max<int64>(2, GlobalMaxBet);
   		case EBettingAction::Pping: return CallDiff > 0 ? CallDiff : 2;
		case EBettingAction::AllIn: return Money;
		default:                    return 0;
		}
	}

	void SetStatusByAction(EBettingAction Action, int64 GlobalMaxBet)
	{
		if (Action == EBettingAction::Die)
		{
			LastActionStatus = TEXT("DIE");
			return;
		}

		if (Action == EBettingAction::Check)
		{
			LastActionStatus = (GlobalMaxBet - BetMoney > 0) ? TEXT("CALL") : TEXT("CHECK");
			return;
		}
		static const TMap<EBettingAction, FString> ActionNames = {
			{ EBettingAction::Call,  TEXT("CALL") },
			{ EBettingAction::Quarter, TEXT("QUARTER") },
			{ EBettingAction::Half,  TEXT("HALF") },
			{ EBettingAction::Ddadang, TEXT("DDADANG") },
			{ EBettingAction::Pping, TEXT("PPING") },
			{ EBettingAction::AllIn, TEXT("ALL-IN") }
		};

		if (ActionNames.Contains(Action))
		{
			LastActionStatus = ActionNames[Action];
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// 5. ī�� ��� (Dealer Struct)

USTRUCT(BlueprintType)
struct FSeotdaDealer
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSeotdaCard> Deck;

public:
	void InitAndShuffle()
	{
		Deck.Empty();
		for (int32 i = 1; i <= 10; i++)
		{
			ECardMonth Month = (ECardMonth)i;
			bool bIsKwang = (i == 1 || i == 3 || i == 8);

			Deck.Add(FSeotdaCard(Month, bIsKwang));
			Deck.Add(FSeotdaCard(Month, false));
		}

		const int32 LastIndex = Deck.Num() - 1;
		for (int32 i = 0; i <= LastIndex; ++i)
		{
			int32 Index = FMath::RandRange(i, LastIndex);
			if (i != Index)
			{
				Deck.Swap(i, Index);
			}
		}
	}

	FSeotdaCard Draw()
	{
		if (Deck.Num() > 0)
		{
			return Deck.Pop();
		}
		return FSeotdaCard();
	}

	int32 GetRemainingCount() const { return Deck.Num(); }
};

//////////////////////////////////////////////////////////////////////////////////////
// 6. ���� ���̺� ���� (Table Struct)

USTRUCT(BlueprintType)
struct FSeotdaTable
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	int64 PotMoney = 0;
	int64 MaxBetMoney = 0;
	int32 BettingCount = 0;
	bool bIsGameInProgress = false;

public:
	void Reset()
	{
		PotMoney = 0;
		MaxBetMoney = 0;
		BettingCount = 0;
		bIsGameInProgress = true;
	}
};