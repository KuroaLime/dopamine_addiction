// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManagerGameMode.h"
#include "TimerManager.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "PlayerManager.h"
#include "Game/InGame/ManagerPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Game/InGame/ManagerGameState.h"
#include "Default/Actor/BaseItem.h"

#define VALIDATE_GS if (!GetGS()) return;
#define VALIDATE_GS_RET(ret) if (!GetGS()) return ret;

// GameState 병합 필요

AManagerGameMode::AManagerGameMode()
{
	SpawnManager = CreateDefaultSubobject<USpawnManagerComponent>(TEXT("SpawnManager"));
}

void AManagerGameMode::BeginPlay()
{
	Super::BeginPlay();

	MGS_Ptr = GetGameState<AManagerGameState>();

	if (GetGS())
	{
		GetGS()->RemainingTime = 60;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] BeginPlay: ManagerGameState is null."));
	}

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->OnPlayerActionEvent.AddUObject(this, &AManagerGameMode::OnPlayerAction);
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] GameMode BeginPlay. Waiting for players... Required=%d"), RequiredPlayerCount);

	// Dedicated Server에서는 BeginPlay에서 게임을 자동 시작하지 않는다.
	// 클라이언트 접속 시 PostLogin에서 인원 수를 확인하고,
	// RequiredPlayerCount명 이상이 모이면 StartSeotdaGame()을 호출한다.

	if (SpawnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ManagerGameMode] BeginPlay: SpawnManager valid."));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("SpawnManager Valid!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ManagerGameMode] BeginPlay: SpawnManager is NULL."));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: SpawnManager is NULL!"));
		}
	}
}

FORCEINLINE AManagerGameState* AManagerGameMode::GetGS() const
{
	if (MGS_Ptr)
	{
		return MGS_Ptr;
	}

	AManagerGameMode* MutableThis = const_cast<AManagerGameMode*>(this);
	MutableThis->MGS_Ptr = Cast<AManagerGameState>(GetWorld()->GetGameState());

	return MGS_Ptr;
}

void AManagerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AManagerPlayerController* MPC = Cast<AManagerPlayerController>(NewPlayer);

	UE_LOG(LogTemp, Warning, TEXT("[DS] PostLogin: %s / HumanPlayers=%d/%d"),
		MPC ? *MPC->GetName() : TEXT("UnknownController"),
		CountConnectedHumanPlayers(),
		RequiredPlayerCount);

	TryStartGameIfReady();
}

void AManagerGameMode::Logout(AController* Exiting)
{
	AManagerPlayerController* MPC = Cast<AManagerPlayerController>(Exiting);

	UE_LOG(LogTemp, Warning, TEXT("[DS] Logout: %s"),
		MPC ? *MPC->GetName() : TEXT("UnknownController"));

	Super::Logout(Exiting);

	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Player left before game start. HumanPlayers=%d/%d"),
			CountConnectedHumanPlayers(),
			RequiredPlayerCount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Player left after game started. Disconnect handling is needed later."));
	}
}

int32 AManagerGameMode::CountConnectedHumanPlayers() const
{
	int32 Count = 0;

	if (!GetWorld())
	{
		return 0;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (Cast<AManagerPlayerController>(It->Get()))
		{
			Count++;
		}
	}

	return Count;
}

void AManagerGameMode::TryStartGameIfReady()
{
	if (bGameStarted)
	{
		return;
	}

	const int32 HumanCount = CountConnectedHumanPlayers();

	if (HumanCount < RequiredPlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Waiting players... %d / %d"),
			HumanCount,
			RequiredPlayerCount);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] Required players joined. Starting Seotda game."));

	StartSeotdaGame();
}

void AManagerGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore action before game start: %s"), *ActionName.ToString());
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
			FString::Printf(TEXT("SERVER RECEIVED ACTION: %s"), *ActionName.ToString()));
	}

	int32 FoundIndex = GetPlayerIndexFromController(Cast<AManagerPlayerController>(Executor));

	if (FoundIndex == INDEX_NONE)
	{
		return;
	}

	if (HandleCardSelection(ActionName, FoundIndex))
	{
		return;
	}

	static const TMap<FName, EBettingAction> ActionMap = {
		{ FName("Check"), EBettingAction::Check },
		{ FName("Call"),  EBettingAction::Call  },
		{ FName("Half"),  EBettingAction::Half  },
		{ FName("Die"),   EBettingAction::Die   },
		{ FName("AllIn"), EBettingAction::AllIn }
	};

	if (const EBettingAction* FoundAction = ActionMap.Find(ActionName))
	{
		ProcessBetting(FoundIndex, *FoundAction);
	}
}

void AManagerGameMode::StartSeotdaGame()
{
	if (bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartSeotdaGame ignored. Game already started."));
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("[Server] Initializing Players..."));
	}

	AddPlayer();

	if (bFillEmptySlotsWithAI)
	{
		AddAI();
	}

	if (Players.Num() < RequiredPlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartSeotdaGame blocked. Players=%d / Required=%d"),
			Players.Num(),
			RequiredPlayerCount);
		return;
	}

	if (Players.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] StartSeotdaGame failed. No players."));
		return;
	}

	bGameStarted = true;

	if (GetGS())
	{
		GetGS()->RemainingTime = 60;
	}

	if (!GetWorldTimerManager().IsTimerActive(RoundTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &AManagerGameMode::RoundTimerTick, 1.0f, true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] Match started. TotalPlayers=%d, FillAI=%s"),
		Players.Num(),
		bFillEmptySlotsWithAI ? TEXT("true") : TEXT("false"));

	StartBattleRoyalePhase();
}

void AManagerGameMode::StartBattleRoyalePhase()
{
	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartBattleRoyalePhase ignored. Game is not started."));
		return;
	}

	CurrentPhase = EMatchPhase::BattleRoyale;

	if (GetGS())
	{
		GetGS()->Table.bIsGameInProgress = false;
		GetGS()->RemainingTime = BattleRoyaleDuration;
	}

	for (FSeotdaPlayerInfo& P : Players)
	{
		P.bHasSelectedCards = false;
		P.BetMoney = 0;
		P.Score = 0;
		P.LastActionStatus = TEXT("BATTLE ROYALE");

		if (P.PC)
		{
			P.PC->Client_SetSeotdaMode(false);
		}
	}

	if (!GetWorldTimerManager().IsTimerActive(RoundTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &AManagerGameMode::RoundTimerTick, 1.0f, true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] Battle Royale Phase Started. Duration=%d"), BattleRoyaleDuration);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[DS] Battle Royale Started. Time=%d"), BattleRoyaleDuration));
	}
}

void AManagerGameMode::StartSeotdaPhase()
{
	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartSeotdaPhase ignored. Game is not started."));
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	CurrentPhase = EMatchPhase::SeotdaCardSelection;

	UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda Phase Started. Loading collected cards..."));

	LoadCollectedCardsForSeotda();

	if (!ValidateSeotdaHands())
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] Seotda phase blocked. Collected cards are not ready."));
		return;
	}

	TableDataReset();
	PlayerDataReset();
	NotifyClientStateReset();

	PrepareSeotdaCardSelection();

	PrepareSeotdaCardSelection();

	UE_LOG(LogTemp, Warning, TEXT("[DS] Waiting for players to select cards."));
}

bool AManagerGameMode::ValidateSeotdaHands() const
{
	if (Players.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] ValidateSeotdaHands failed. Players is empty."));
		return false;
	}

	bool bHasPlayablePlayer = false;

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const FSeotdaPlayerInfo& P = Players[i];

		// 돈이 없는 플레이어는 이번 섯다 판에서 제외될 수 있으므로 카드 검사를 넘긴다.
		if (P.Money <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] Player has no money. Skip card validation. Index=%d, Name=%s"),
				i,
				*P.PlayerName);
			continue;
		}

		bHasPlayablePlayer = true;

		if (P.Hand.Num() != RequiredSeotdaCardCount)
		{
			UE_LOG(LogTemp, Error, TEXT("[DS] Invalid seotda hand. Index=%d, Name=%s, Cards=%d, Required=%d"),
				i,
				*P.PlayerName,
				P.Hand.Num(),
				RequiredSeotdaCardCount);

			return false;
		}
	}

	if (!bHasPlayablePlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] ValidateSeotdaHands failed. No playable player."));
		return false;
	}

	return true;
}

void AManagerGameMode::PrepareSeotdaCardSelection()
{
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		FSeotdaPlayerInfo& P = Players[i];

		P.bHasSelectedCards = false;

		if (P.Money <= 0)
		{
			P.bIsFolded = true;
			P.LastActionStatus = TEXT("NO MONEY");
			continue;
		}

		if (P.bIsAI)
		{
			ExecuteAIHandSelection(i);
			P.bHasSelectedCards = true;
		}
		else
		{
			SendHandDataToClient(i);

			if (P.PC)
			{
				P.PC->Client_SetSeotdaMode(true);
			}
		}
	}

	UpdateGameStatusHUD();

	UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda card selection prepared."));
}

void AManagerGameMode::LoadCollectedCardsForSeotda()
{
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		FSeotdaPlayerInfo& P = Players[i];

		// AI는 일단 제외. 테스트용 AI를 쓸 거면 별도 처리 필요.
		if (P.bIsAI)
		{
			continue;
		}

		P.Hand.Empty();

		if (!P.PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] LoadCollectedCardsForSeotda: PC is null. Index=%d"), i);
			continue;
		}

		APawn* Pawn = P.PC->GetPawn();

		if (!Pawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] LoadCollectedCardsForSeotda: Pawn is null. Index=%d"), i);
			continue;
		}

		AManagerCharacter* Character = Cast<AManagerCharacter>(Pawn);

		if (!Character)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] LoadCollectedCardsForSeotda: Character cast failed. Index=%d, Pawn=%s"),
				i,
				*Pawn->GetName());
			continue;
		}

		for (ABaseItem* Item : Character->Inventory)
		{
			if (!Item)
			{
				continue;
			}

			if (Item->GetItemType() != EPickableType::Card)
			{
				continue;
			}

			if (Item->GetCardMonth() == ECardMonth::None)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DS] Invalid card item. CardMonth is None. Item=%s"),
					*Item->GetName());
				continue;
			}

			FSeotdaCard NewCard;
			NewCard.Month = Item->GetCardMonth();
			NewCard.bIsKwang = Item->IsKwangCard();

			P.Hand.Add(NewCard);
		}

		UE_LOG(LogTemp, Warning, TEXT("[DS] Loaded collected cards. Player=%s, CardCount=%d"),
			*P.PlayerName,
			P.Hand.Num());
	}
}

bool AManagerGameMode::AreAllSeotdaSelectionsComplete() const
{
	for (const FSeotdaPlayerInfo& P : Players)
	{
		// 돈이 없거나 이미 폴드 처리된 플레이어는 이번 섯다 판에서 제외한다.
		if (P.Money <= 0 || P.bIsFolded)
		{
			continue;
		}

		if (!P.bHasSelectedCards)
		{
			return false;
		}
	}

	return true;
}

void AManagerGameMode::StartSeotdaBettingPhase()
{
	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartSeotdaBettingPhase ignored. Game is not started."));
		return;
	}

	if (CurrentPhase != EMatchPhase::SeotdaCardSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartSeotdaBettingPhase ignored. CurrentPhase=%d"),
			static_cast<int32>(CurrentPhase));
		return;
	}

	CurrentPhase = EMatchPhase::SeotdaBetting;

	if (GetGS())
	{
		GetGS()->Table.bIsGameInProgress = true;
	}

	FirstBetterIndex = GetNextValidPlayerIndex(FirstBetterIndex);
	CurrentTurnIndex = FirstBetterIndex;

	if (CurrentTurnIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] StartSeotdaBettingPhase failed. No valid player."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda Betting Phase Started. FirstBetterIndex=%d"),
		FirstBetterIndex);

	UpdateGameStatusHUD();
	StartCurrentTurn();
}

void AManagerGameMode::AddPlayer()
{
	Players.Empty();

	int32 HumanIdx = 1;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AManagerPlayerController* PC = Cast<AManagerPlayerController>(It->Get()))
		{
			FSeotdaPlayerInfo NewHuman;
			NewHuman.PlayerName = FString::Printf(TEXT("User_%d"), HumanIdx++);
			NewHuman.Money = 10000;
			NewHuman.bIsAI = false;
			NewHuman.PC = PC;

			Players.Add(NewHuman);

			PC->Client_SetSeotdaMode(true);
		}
	}
}

void AManagerGameMode::AddAI()
{
	int32 CurrentHumanCount = Players.Num();

	for (int32 i = CurrentHumanCount + 1; i <= 4; ++i)
	{
		FSeotdaPlayerInfo AI;
		AI.PlayerName = FString::Printf(TEXT("AI_Bot_%d"), i);
		AI.Money = 10000;
		AI.bIsAI = true;
		AI.Style = static_cast<EAIStyle>(FMath::RandRange(0, 3));
		AI.PC = nullptr;

		Players.Add(AI);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// 게임 흐름 관리

void AManagerGameMode::StartNextRound()
{
	if (!bGameStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] StartNextRound ignored. Game is not started."));
		return;
	}

	if (Players.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] StartNextRound failed. Players is empty."));
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("!!! StartNextRound Called !!!")));
	}

	TableDataReset();
	PlayerDataReset();

	FirstBetterIndex = (FirstBetterIndex + 1) % Players.Num();
	CurrentTurnIndex = FirstBetterIndex;

	NotifyClientStateReset();
	DealCards();
	UpdateGameStatusHUD();
	StartCurrentTurn();
}

void AManagerGameMode::StartCurrentTurn()
{
	VALIDATE_GS

		if (!GetGS()->Table.bIsGameInProgress)
		{
			return;
		}

	if (!Players.IsValidIndex(CurrentTurnIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("[DS] StartCurrentTurn failed. Invalid CurrentTurnIndex=%d"), CurrentTurnIndex);
		return;
	}

	FSeotdaPlayerInfo& CurrentP = Players[CurrentTurnIndex];

	if (CurrentP.bIsAI)
	{
		RunAILogicDelayed(CurrentTurnIndex, FMath::RandRange(1.0f, 2.0f));
	}
	else
	{
		if (CurrentP.PC)
		{
			CurrentP.PC->Client_SetSeotdaMode(true);
		}
	}
}

void AManagerGameMode::NextTurn()
{
	CurrentTurnIndex = GetNextValidPlayerIndex(CurrentTurnIndex);
	UpdateGameStatusHUD();
	StartCurrentTurn();
}

int32 AManagerGameMode::GetNextValidPlayerIndex(int32 StartingIndex)
{
	if (Players.Num() <= 0)
	{
		return INDEX_NONE;
	}

	int32 NextIndex = StartingIndex;

	do
	{
		NextIndex = (NextIndex + 1) % Players.Num();
	} while ((Players[NextIndex].bIsFolded || Players[NextIndex].Money <= 0) && NextIndex != StartingIndex);

	return NextIndex;
}

void AManagerGameMode::DealCards()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("!!! DealCards Called !!!")));
	}

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		FSeotdaPlayerInfo& P = Players[i];

		P.Hand.Empty();

		for (int32 j = 0; j < 3; ++j)
		{
			P.Hand.Add(CardDealer.Draw());
		}

		P.Score = 0;

		if (P.bIsAI)
		{
			ExecuteAIHandSelection(i);
		}
		else
		{
			SendHandDataToClient(i);
		}
	}
}

void AManagerGameMode::DetermineWinner()
{
	VALIDATE_GS

		CurrentPhase = EMatchPhase::SeotdaResult;
	GetGS()->Table.bIsGameInProgress = false;

	TArray<int32> WinnerIndices = GetWinnerIndices();

	DistributePot(WinnerIndices);

	DisplayFinalResults(WinnerIndices);
	UpdateGameStatusHUD();

	UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda Result Phase. Next Battle Royale will start soon."));

	ScheduleNextBattleRoyale(5.0f);
}

TArray<int32> AManagerGameMode::GetWinnerIndices()
{
	int32 MaxScore = -1;
	TArray<int32> Indices;

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (Players[i].bIsFolded)
		{
			continue;
		}

		if (Players[i].Score > MaxScore)
		{
			MaxScore = Players[i].Score;
			Indices.Empty();
			Indices.Add(i);
		}
		else if (Players[i].Score == MaxScore)
		{
			Indices.Add(i);
		}
	}

	return Indices;
}

void AManagerGameMode::DistributePot(const TArray<int32>& WinnerIndices)
{
	VALIDATE_GS

		if (WinnerIndices.Num() == 0)
		{
			return;
		}

	int64 TotalPot = GetGS()->Table.PotMoney;
	int64 ShareMoney = TotalPot / WinnerIndices.Num();
	int64 Remainder = TotalPot % WinnerIndices.Num();

	for (int32 Idx : WinnerIndices)
	{
		Players[Idx].Money += ShareMoney;
	}

	if (Remainder > 0 && WinnerIndices.Num() > 0)
	{
		Players[WinnerIndices[0]].Money += Remainder;
	}

	GetGS()->Table.PotMoney = 0;
}

void AManagerGameMode::ScheduleNextRound(float Delay)
{
	FTimerHandle WaitHandle;
	GetWorld()->GetTimerManager().SetTimer(WaitHandle, this, &AManagerGameMode::StartNextRound, Delay, false);
}

void AManagerGameMode::ScheduleNextBattleRoyale(float Delay)
{
	FTimerHandle WaitHandle;

	GetWorld()->GetTimerManager().SetTimer(
		WaitHandle,
		this,
		&AManagerGameMode::StartBattleRoyalePhase,
		Delay,
		false
	);
}

//////////////////////////////////////////////////////////////////////////////////////
// 베팅 및 카드 선택 처리

void AManagerGameMode::ProcessBetting(int32 PlayerIndex, EBettingAction Action)
{
	VALIDATE_GS

		if (CurrentPhase != EMatchPhase::SeotdaBetting)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore betting. CurrentPhase=%d"),
				static_cast<int32>(CurrentPhase));
			return;
		}

	if (!GetGS()->Table.bIsGameInProgress || PlayerIndex != CurrentTurnIndex)
	{
		return;
	}

	if (!Players.IsValidIndex(PlayerIndex))
	{
		return;
	}

	FSeotdaPlayerInfo& P = Players[PlayerIndex];
	int64 PreviousMaxBet = GetGS()->Table.MaxBetMoney;

	P.SetStatusByAction(Action, GetGS()->Table.MaxBetMoney);

	if (Action == EBettingAction::Die)
	{
		P.bIsFolded = true;
	}
	else
	{
		int64 PayAmount = P.CalculateRequiredPay(Action, GetGS()->Table.MaxBetMoney, GetGS()->Table.PotMoney);
		P.Pay(PayAmount, GetGS()->Table.PotMoney);
		GetGS()->Table.MaxBetMoney = FMath::Max(GetGS()->Table.MaxBetMoney, P.BetMoney);
	}

	if (GetGS()->Table.MaxBetMoney > PreviousMaxBet)
	{
		GetGS()->Table.BettingCount = 0;
	}

	GetGS()->Table.BettingCount++;

	UpdateGameStatusHUD();

	if (CheckBettingRoundEnd())
	{
		DetermineWinner();
	}
	else
	{
		NextTurn();
	}
}

bool AManagerGameMode::CheckBettingRoundEnd()
{
	VALIDATE_GS_RET(false)

		int32 ActivePlayers = 0;

	for (const auto& P : Players)
	{
		if (!P.bIsFolded && P.Money > 0)
		{
			ActivePlayers++;
		}
	}

	if (GetGS()->Table.BettingCount < ActivePlayers)
	{
		return false;
	}

	for (const auto& P : Players)
	{
		if (P.bIsFolded || P.Money <= 0)
		{
			continue;
		}

		if (P.BetMoney != GetGS()->Table.MaxBetMoney)
		{
			return false;
		}
	}

	return true;
}

void AManagerGameMode::ProcessCardSelection(int32 PlayerIndex, int32 Index1, int32 Index2)
{
	if (CurrentPhase != EMatchPhase::SeotdaCardSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore card selection. CurrentPhase=%d"),
			static_cast<int32>(CurrentPhase));
		return;
	}

	if (!Players.IsValidIndex(PlayerIndex))
	{
		return;
	}

	FSeotdaPlayerInfo& P = Players[PlayerIndex];

	if (P.bIsFolded || P.Money <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore card selection. Player cannot play. Index=%d"), PlayerIndex);
		return;
	}

	if (P.bHasSelectedCards)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore duplicated card selection. Index=%d"), PlayerIndex);
		return;
	}

	if (Index1 == Index2)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore invalid card selection. Same index. Index=%d"), PlayerIndex);
		return;
	}

	if (!P.Hand.IsValidIndex(Index1) || !P.Hand.IsValidIndex(Index2))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ignore invalid card selection. Player=%d, Index1=%d, Index2=%d, HandNum=%d"),
			PlayerIndex,
			Index1,
			Index2,
			P.Hand.Num());
		return;
	}

	FSeotdaCard C1 = P.Hand[Index1];
	FSeotdaCard C2 = P.Hand[Index2];

	P.Score = FSeotdaCard::GetScore(C1, C2);
	P.bHasSelectedCards = true;

	UE_LOG(LogTemp, Warning, TEXT("[DS] Player selected cards. Index=%d, Card1=%s, Card2=%s, Score=%d"),
		PlayerIndex,
		*C1.ToString(),
		*C2.ToString(),
		P.Score);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("Server: Player %d picked %s & %s (Score: %d)"),
				PlayerIndex,
				*C1.ToString(),
				*C2.ToString(),
				P.Score));
	}

	if (AreAllSeotdaSelectionsComplete())
	{
		StartSeotdaBettingPhase();
	}
}

bool AManagerGameMode::HandleCardSelection(FName ActionName, int32 PlayerIndex)
{
	FString ActionStr = ActionName.ToString();

	if (!ActionStr.StartsWith(TEXT("SelectCards_")))
	{
		return false;
	}

	TArray<FString> Parts;
	ActionStr.ParseIntoArray(Parts, TEXT("_"), true);

	if (Parts.Num() == 3)
	{
		int32 Idx1 = FCString::Atoi(*Parts[1]);
		int32 Idx2 = FCString::Atoi(*Parts[2]);

		ProcessCardSelection(PlayerIndex, Idx1, Idx2);

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////
// 통신 및 UI 갱신

AManagerPlayerController* AManagerGameMode::GetPlayerControllerFromIndex(int32 Index)
{
	if (!Players.IsValidIndex(Index))
	{
		return nullptr;
	}

	return Players[Index].PC;
}

void AManagerGameMode::SendHandDataToClient(int32 PlayerIndex)
{
	if (!Players.IsValidIndex(PlayerIndex))
	{
		return;
	}

	if (AManagerPlayerController* PC = GetPlayerControllerFromIndex(PlayerIndex))
	{
		TArray<FString> NameList;

		for (const auto& C : Players[PlayerIndex].Hand)
		{
			NameList.Add(C.ToString());
		}

		PC->Client_SetHandInfo(NameList);
	}
}

void AManagerGameMode::NotifyClientStateReset()
{
	for (const auto& P : Players)
	{
		if (P.PC)
		{
			P.PC->Client_StateReset();
		}
	}
}

int32 AManagerGameMode::GetPlayerIndexFromController(AManagerPlayerController* PC) const
{
	if (!PC)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (Players[i].PC == PC)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void AManagerGameMode::TableDataReset()
{
	VALIDATE_GS

		CardDealer.InitAndShuffle();
	GetGS()->Table.Reset();
}

void AManagerGameMode::PlayerDataReset()
{
	VALIDATE_GS

		for (auto& P : Players)
		{
			P.ResetForNewRound();

			// 보유 금액은 다음 라운드로 유지한다.
			// 돈이 0 이하인 플레이어를 10000원으로 다시 살리지 않는다.
			if (P.Money <= 0)
			{
				P.bIsFolded = true;
				P.LastActionStatus = TEXT("NO MONEY");
				continue;
			}

			// 참가비/기본 베팅금. 필요 없으면 나중에 0 또는 제거 가능.
			P.Pay(100, GetGS()->Table.PotMoney);
		}
}

void AManagerGameMode::UpdateGameStatusHUD()
{
	VALIDATE_GS

		if (!GEngine)
		{
			return;
		}

	FString GameInfo = FString::Printf(TEXT("TOTAL POT: %lld | MAX BET: %lld"), GetGS()->Table.PotMoney, GetGS()->Table.MaxBetMoney);
	GEngine->AddOnScreenDebugMessage(900, 1000.0f, FColor::Yellow, GameInfo);

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const FSeotdaPlayerInfo& P = Players[i];

		FColor TextColor = FColor::White;

		if (P.bIsFolded)
		{
			TextColor = FColor::Silver;
		}
		else if (i == CurrentTurnIndex && GetGS()->Table.bIsGameInProgress)
		{
			TextColor = FColor::Green;
		}

		FString TurnMarker = (i == CurrentTurnIndex && GetGS()->Table.bIsGameInProgress) ? TEXT(">> ") : TEXT("   ");

		FString PlayerMsg = FString::Printf(TEXT("%s[%s] Money: %lld | Bet: %lld | Last: %s"),
			*TurnMarker,
			*P.PlayerName,
			P.Money,
			P.BetMoney,
			*P.LastActionStatus);

		GEngine->AddOnScreenDebugMessage(901 + i, 1000.0f, TextColor, PlayerMsg);
	}
}

void AManagerGameMode::DisplayFinalResults(const TArray<int32> WinnerIndices)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(800, 10.0f, FColor::Yellow, TEXT("========== [ FINAL RESULT ] =========="));

		for (int32 i = 0; i < Players.Num(); ++i)
		{
			FSeotdaPlayerInfo& P = Players[i];
			FString StatusStr;
			FColor Color = FColor::White;

			if (P.bIsFolded)
			{
				StatusStr = TEXT("DIE");
				Color = FColor::Silver;
			}
			else if (WinnerIndices.Contains(i))
			{
				StatusStr = FString::Printf(TEXT("WINNER! (Score: %d)"), P.Score);
				Color = FColor::Yellow;
			}
			else
			{
				StatusStr = FString::Printf(TEXT("LOSE (Score: %d)"), P.Score);
				Color = FColor::White;
			}

			FString ResultMsg = FString::Printf(TEXT("[%s] %s"), *P.PlayerName, *StatusStr);
			GEngine->AddOnScreenDebugMessage(801 + i, 10.0f, Color, ResultMsg);
		}

		GEngine->AddOnScreenDebugMessage(810, 10.0f, FColor::Cyan, TEXT("Next Game starting in 5 seconds..."));
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// AI 의사결정 관련

EBettingAction AManagerGameMode::DetermineAIDecision(int32 PlayerIndex)
{
	VALIDATE_GS_RET(EBettingAction::Die)

		if (!Players.IsValidIndex(PlayerIndex))
		{
			return EBettingAction::Die;
		}

	FSeotdaPlayerInfo& Bot = Players[PlayerIndex];
	int32 Score = Bot.Score;
	EAIStyle Style = Bot.Style;

	int64 CallDiff = GetGS()->Table.MaxBetMoney - Bot.BetMoney;
	float RiskRatio = (Bot.Money > 0) ? static_cast<float>(CallDiff) / static_cast<float>(Bot.Money) : 1.0f;

	if (CallDiff <= 0)
	{
		return EBettingAction::Check;
	}

	if (Bot.Money <= 0)
	{
		return EBettingAction::AllIn;
	}

	switch (Style)
	{
	case EAIStyle::Conservative:
		if (Score >= 2000) return EBettingAction::Half;
		if (Score >= 1000 || (Score >= 7 && RiskRatio < 0.1f)) return EBettingAction::Call;
		return EBettingAction::Die;

	case EAIStyle::Balanced:
		if (Score >= 1000) return EBettingAction::Half;
		if (Score >= 7) return EBettingAction::Call;
		if (Score >= 4 && RiskRatio < 0.2f) return EBettingAction::Call;
		return EBettingAction::Die;

	case EAIStyle::Aggressive:
		if (Score >= 1000 || Score >= 8) return EBettingAction::Half;
		if (Score >= 5 || RiskRatio < 0.4f) return EBettingAction::Call;
		return EBettingAction::Die;

	case EAIStyle::Gambler:
		if (FMath::RandRange(1, 100) <= 15) return EBettingAction::Half;
		if (Score >= 2000) return EBettingAction::AllIn;
		if (Score >= 1000) return EBettingAction::Half;
		if (Score >= 1) return EBettingAction::Call;
		return EBettingAction::Die;
	}

	return EBettingAction::Die;
}

void AManagerGameMode::ExecuteAIHandSelection(int32 PlayerIndex)
{
	if (!Players.IsValidIndex(PlayerIndex))
	{
		return;
	}

	FSeotdaPlayerInfo& Bot = Players[PlayerIndex];

	if (Bot.Hand.Num() != 3)
	{
		return;
	}

	int32 Score01 = FSeotdaCard::GetScore(Bot.Hand[0], Bot.Hand[1]);
	int32 Score02 = FSeotdaCard::GetScore(Bot.Hand[0], Bot.Hand[2]);
	int32 Score12 = FSeotdaCard::GetScore(Bot.Hand[1], Bot.Hand[2]);

	int32 BestScore = FMath::Max3(Score01, Score02, Score12);

	Bot.Score = BestScore;
}

void AManagerGameMode::RunAILogicDelayed(int32 PlayerIndex, float Delay)
{
	FTimerHandle AIWaitHandle;

	GetWorld()->GetTimerManager().SetTimer(AIWaitHandle, [this, PlayerIndex]()
		{
			if (!GetGS() || !GetGS()->Table.bIsGameInProgress || CurrentTurnIndex != PlayerIndex)
			{
				return;
			}

			EBettingAction Decision = DetermineAIDecision(PlayerIndex);

			ProcessBetting(PlayerIndex, Decision);

		}, Delay, false);
}

void AManagerGameMode::RoundTimerTick()
{
	if (!bGameStarted)
	{
		return;
	}

	VALIDATE_GS

		MGS_Ptr->RemainingTime--;

	if (MGS_Ptr->OnTimeUpdated.IsBound())
	{
		MGS_Ptr->OnTimeUpdated.Broadcast(MGS_Ptr->RemainingTime);
	}

	if (MGS_Ptr->RemainingTime <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Timer End. CurrentPhase=%d"), static_cast<int32>(CurrentPhase));

		GetWorldTimerManager().ClearTimer(RoundTimerHandle);

		if (CurrentPhase == EMatchPhase::BattleRoyale)
		{
			StartSeotdaPhase();
			return;
		}

		if (CurrentPhase == EMatchPhase::SeotdaBetting)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda betting timer ended. Timeout handling is needed later."));
			return;
		}
	}
}

AActor* AManagerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (SpawnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("Initialize Random Spawn..."));

		if (SpawnManager->GetAvailableSpawnCount() == 0)
		{
			SpawnManager->InitializeSpawnPoints();
		}

		AA_Spawn* RandomSpawn = SpawnManager->GetUniqueRandomSpawnActor();

		if (RandomSpawn)
		{
			return RandomSpawn;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AManagerGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform OffsetTransform = SpawnTransform;
	FVector NewLocation = OffsetTransform.GetLocation() + FVector(0.0f, 0.0f, 100.0f);

	OffsetTransform.SetLocation(NewLocation);

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OffsetTransform);
}