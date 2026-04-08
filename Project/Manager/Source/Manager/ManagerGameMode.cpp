// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManagerGameMode.h"
#include "TimerManager.h"
#include "Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Actor/Spawn/A_Spawn.h"
#include "PlayerManager.h"
#include "ManagerPlayerController.h"
#include "ManagerGameState.h"

#define VALIDATE_GS if (!GetGS()) return;
#define VALIDATE_GS_RET(ret) if (!GetGS()) return ret;

// GameState 병합 필요

AManagerGameMode::AManagerGameMode()
{
	SpawnManager = CreateDefaultSubobject<USpawnManagerComponent>(TEXT("SpawnManager"));

}
void AManagerGameMode::BeginPlay() {
	Super::BeginPlay();

	MGS_Ptr = GetGameState<AManagerGameState>();
	MGS_Ptr->RemainingTime = 60;

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &AManagerGameMode::RoundTimerTick, 1.0f, true);

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->OnPlayerActionEvent.AddUObject(this, &AManagerGameMode::OnPlayerAction);
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[Server] GameMode BeginPlay! Waiting 2sec..."));

	FTimerHandle WaitHandle;
	GetWorld()->GetTimerManager().SetTimer(WaitHandle, this, &AManagerGameMode::StartSeotdaGame, 2.0f, false);

	// main
	if (SpawnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ManagerGameMode] BeginPlay: SpawnManager�� ���������� �����մϴ�."));
		if (GEngine)
		{
			// ȭ�� ���� ���� �ʷϻ� �ؽ�Ʈ�� ����ݴϴ�.
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("SpawnManager Valid!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ManagerGameMode] BeginPlay: SpawnManager�� NULL�Դϴ�!!!"));
		if (GEngine)
		{
			// ȭ�� ���� ���� ������ �ؽ�Ʈ�� ����ݴϴ�.
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: SpawnManager is NULL!"));
		}
	}

}

void AManagerGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
		FString::Printf(TEXT("SERVER RECEIVED ACTION: %s"), *ActionName.ToString()));

	int32 FoundIndex = GetPlayerIndexFromController(Cast<AManagerPlayerController>(Executor));

	if (FoundIndex == INDEX_NONE) return;
	if (HandleCardSelection(ActionName, FoundIndex)) return;

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
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("[Server] Initializing Players..."));

	AddPlayer();
	AddAI();

	StartNextRound();
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
			PC->SetSeotdaMode(true);
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
		AI.Style = (EAIStyle)FMath::RandRange(0, 3);
		AI.PC = nullptr;

		Players.Add(AI);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// ���� �帧 ����

void AManagerGameMode::StartNextRound()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("!!! StartNextRound Called !!!")));

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
	if (!GetGS()->Table.bIsGameInProgress) return;

	FSeotdaPlayerInfo& CurrentP = Players[CurrentTurnIndex];

	if (CurrentP.bIsAI)
	{
		RunAILogicDelayed(CurrentTurnIndex, FMath::RandRange(1.0f, 2.0f));
	}
	else
	{
		if (CurrentP.PC)
		{
			CurrentP.PC->SetSeotdaMode(true);
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
	int32 NextIndex = StartingIndex;
	do {
		NextIndex = (NextIndex + 1) % Players.Num();
	} while ((Players[NextIndex].bIsFolded || Players[NextIndex].Money <= 0) && NextIndex != StartingIndex);

	return NextIndex;
}

void AManagerGameMode::DealCards()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("!!! DealCards Called !!!")));


	for (int32 i = 0; i < Players.Num(); ++i)
	{
		FSeotdaPlayerInfo& P = Players[i];
		P.Hand.Empty();
		for (int j = 0; j < 3; ++j) P.Hand.Add(CardDealer.Draw());
		P.Score = 0;

		if (P.bIsAI) ExecuteAIHandSelection(i);
		else SendHandDataToClient(i);
	}
}

void AManagerGameMode::DetermineWinner()
{
	TArray<int32> WinnerIndices = GetWinnerIndices();

	DistributePot(WinnerIndices);

	DisplayFinalResults(WinnerIndices);
	UpdateGameStatusHUD();

	ScheduleNextRound(5.0f);
}

TArray<int32> AManagerGameMode::GetWinnerIndices()
{
	int32 MaxScore = -1;
	TArray<int32> Indices;

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (Players[i].bIsFolded) continue;

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
	if (WinnerIndices.Num() == 0) return;

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

//////////////////////////////////////////////////////////////////////////////////////
// 3. �ٽ� ���� ����

void AManagerGameMode::ProcessBetting(int32 PlayerIndex, EBettingAction Action)
{
	VALIDATE_GS
	if (!GetGS()->Table.bIsGameInProgress || PlayerIndex != CurrentTurnIndex) return;

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

	if (GetGS()->Table.MaxBetMoney > PreviousMaxBet) GetGS()->Table.BettingCount = 0;
	GetGS()->Table.BettingCount++;

	UpdateGameStatusHUD();

	if (CheckBettingRoundEnd())
	{
		DetermineWinner();
		GetGS()->Table.bIsGameInProgress = false;
	}
	else NextTurn();
}

bool AManagerGameMode::CheckBettingRoundEnd()
{
	VALIDATE_GS_RET(false);

	int32 ActivePlayers = 0;
	for (const auto& P : Players) if (!P.bIsFolded && P.Money > 0) ActivePlayers++;

	if (GetGS()->Table.BettingCount < ActivePlayers) return false;

	for (const auto& P : Players)
	{
		if (P.bIsFolded || P.Money <= 0) continue;
		if (P.BetMoney != GetGS()->Table.MaxBetMoney) return false;
	}
	return true;
}

void AManagerGameMode::ProcessCardSelection(int32 PlayerIndex, int32 Index1, int32 Index2)
{
	FSeotdaPlayerInfo& P = Players[PlayerIndex];

	if (!P.Hand.IsValidIndex(Index1) || !P.Hand.IsValidIndex(Index2)) return;

	FSeotdaCard C1 = P.Hand[Index1];
	FSeotdaCard C2 = P.Hand[Index2];
	P.Score = FSeotdaCard::GetScore(C1, C2);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("Server: You picked %s & %s (Score: %d)"), *C1.ToString(), *C2.ToString(), P.Score));
}

bool AManagerGameMode::HandleCardSelection(FName ActionName, int32 PlayerIndex)
{
	FString ActionStr = ActionName.ToString();
	if (!ActionStr.StartsWith(TEXT("SelectCards_"))) return false;

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
// 4. ��� �� UI ����

AManagerPlayerController* AManagerGameMode::GetPlayerControllerFromIndex(int32 Index)
{
	return Cast<AManagerPlayerController>(GetWorld()->GetFirstPlayerController());
}

void AManagerGameMode::SendHandDataToClient(int32 PlayerIndex)
{
	if (auto* PC = GetPlayerControllerFromIndex(PlayerIndex))
	{
		TArray<FString> NameList;
		for (const auto& C : Players[PlayerIndex].Hand) NameList.Add(C.ToString());

		PC->Client_SetHandInfo(NameList);
	}
}

void AManagerGameMode::NotifyClientStateReset()
{
	for (const auto& P : Players)
	{
		if (P.PC) P.PC->Client_StateReset();
	}
}

int32 AManagerGameMode::GetPlayerIndexFromController(AManagerPlayerController* PC) const
{
	if (!PC) return INDEX_NONE;

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
		if (P.Money <= 0) P.Money = 10000;
		P.Pay(100, GetGS()->Table.PotMoney);
	}
}

void AManagerGameMode::UpdateGameStatusHUD()
{
	VALIDATE_GS
	if (!GEngine) return;

	FString GameInfo = FString::Printf(TEXT("TOTAL POT: %lld | MAX BET: %lld"), GetGS()->Table.PotMoney, GetGS()->Table.MaxBetMoney);
	GEngine->AddOnScreenDebugMessage(900, 1000.0f, FColor::Yellow, GameInfo);

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const FSeotdaPlayerInfo& P = Players[i];

		FColor TextColor = FColor::White;
		if (P.bIsFolded) TextColor = FColor::Silver;
		else if (i == CurrentTurnIndex && GetGS()->Table.bIsGameInProgress) TextColor = FColor::Green;

		FString TurnMarker = (i == CurrentTurnIndex && GetGS()->Table.bIsGameInProgress) ? TEXT("�� ") : TEXT("    ");

		FString PlayerMsg = FString::Printf(TEXT("%s[%s] Money: %lld | Bet: %lld | Last: %s"),
			*TurnMarker, *P.PlayerName, P.Money, P.BetMoney, *P.LastActionStatus);

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
// 5. AI �ǻ���� ����

EBettingAction AManagerGameMode::DetermineAIDecision(int32 PlayerIndex)
{
	VALIDATE_GS_RET(EBettingAction::Die);

	FSeotdaPlayerInfo& Bot = Players[PlayerIndex];
	int32 Score = Bot.Score;
	EAIStyle Style = Bot.Style;

	int64 CallDiff = GetGS()->Table.MaxBetMoney - Bot.BetMoney;
	float RiskRatio = (Bot.Money > 0) ? (float)CallDiff / (float)Bot.Money : 1.0f;

	if (CallDiff <= 0) return EBettingAction::Check;
	if (Bot.Money <= 0) return EBettingAction::AllIn;

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
	FSeotdaPlayerInfo& Bot = Players[PlayerIndex];

	if (Bot.Hand.Num() != 3) return;

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
			if (!GetGS() || !GetGS()->Table.bIsGameInProgress || CurrentTurnIndex != PlayerIndex) return;

			EBettingAction Decision = DetermineAIDecision(PlayerIndex);

			ProcessBetting(PlayerIndex, Decision);

		}, Delay, false);

}

void AManagerGameMode::RoundTimerTick() {
	VALIDATE_GS

	MGS_Ptr->RemainingTime--;
	if (MGS_Ptr->OnTimeUpdated.IsBound())
		MGS_Ptr->OnTimeUpdated.Broadcast(MGS_Ptr->RemainingTime);

	if (MGS_Ptr->RemainingTime <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Round End.."));
		GetWorldTimerManager().ClearTimer(RoundTimerHandle);
		if (SpawnManager)
		{
			int32 RandomID = SpawnManager->GetRandomSpawnID();
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PC = Iterator->Get();
				if (PC && PC->GetPawn())
				{
					FVector SpawnLoc;

					if (SpawnManager->GetSpawnLocation(RandomID, SpawnLoc))
					{
						PC->GetPawn()->SetActorLocation(SpawnLoc + FVector(0.0f, 0, 50.0f));
						UE_LOG(LogTemp, Warning, TEXT("Player %s Moving Complete (ID: %d)"), *PC->GetName(), RandomID);
					}
				}
			}
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