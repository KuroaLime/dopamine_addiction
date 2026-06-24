#include "Game/InGame/MainGameMode.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Containers/StringConv.h"

#include "Game/InGame/PhaseStrategy.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainGameState.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"

AMainGameMode::AMainGameMode()
{
    CurrentStrategy = nullptr;
    SpawnManager = CreateDefaultSubobject<USpawnManagerComponent>(TEXT("SpawnManager"));
}

void AMainGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    const FString RoomIdStr = UGameplayStatics::ParseOption(Options, TEXT("RoomId"));
    if (!RoomIdStr.IsEmpty())
    {
        DediRoomId = FCString::Atoi(*RoomIdStr);
    }

    const FString RequiredPlayersStr = UGameplayStatics::ParseOption(Options, TEXT("RequiredPlayers"));
    if (!RequiredPlayersStr.IsEmpty())
    {
        const int32 ParsedRequiredPlayers = FCString::Atoi(*RequiredPlayersStr);
        if (ParsedRequiredPlayers > 0)
        {
            RequiredPlayerCount = ParsedRequiredPlayers;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main InitGame Map=%s Options=%s RoomId=%d RequiredPlayers=%d"),
        *MapName,
        *Options,
        DediRoomId,
        RequiredPlayerCount);
}

void AMainGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    const FString Ticket = UGameplayStatics::ParseOption(Options, TEXT("ticket"));

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main PreLogin Address=%s Ticket=%s Options=%s"),
        *Address,
        Ticket.IsEmpty() ? TEXT("<EMPTY>") : *Ticket,
        *Options);

    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main PreLogin rejected by Super. Error=%s"), *ErrorMessage);
        return;
    }

    //if (Ticket.IsEmpty())
    //{
    //    ErrorMessage = TEXT("MissingTicket");
    //    UE_LOG(LogTemp, Warning, TEXT("[DS] Main PreLogin rejected. Error=%s"), *ErrorMessage);
    //    return;
    //}
}

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    InitStrategy();

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main BeginPlay RoomId=%d RequiredPlayers=%d InitialPhase=%d Strategies=%d DebugPhase=%d RealReady=%d RealBattle=%d RealTransition=%d RealCard=%d RealResult=%d DebugReady=%d DebugBattle=%d DebugTransition=%d DebugCard=%d DebugResult=%d"),
        DediRoomId,
        RequiredPlayerCount,
        static_cast<int32>(InitialPhase),
        StrategyMap.Num(),
        bUseDebugPhaseDurations ? 1 : 0,
        RealReadyDuration,
        RealBattleRoyaleDuration,
        RealTransitionDuration,
        RealCardGameDuration,
        RealResultDuration,
        DebugReadyDuration,
        DebugBattleRoyaleDuration,
        DebugTransitionDuration,
        DebugCardGameDuration,
        DebugResultDuration);
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main PostLogin Controller=%s RoomId=%d HumanPlayers=%d/%d"),
        NewPlayer ? *NewPlayer->GetName() : TEXT("<NULL>"),
        DediRoomId,
        CountConnectedHumanPlayers(),
        RequiredPlayerCount);

    TryStartGameIfReady();
}

void AMainGameMode::Logout(AController* Exiting)
{
    UE_LOG(LogTemp, Warning, TEXT("[DS] Main Logout Controller=%s RoomId=%d HumanPlayersBeforeSuper=%d/%d"),
        Exiting ? *Exiting->GetName() : TEXT("<NULL>"),
        DediRoomId,
        CountConnectedHumanPlayers(),
        RequiredPlayerCount);

    Super::Logout(Exiting);

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main Logout Complete HumanPlayers=%d/%d GameStarted=%d Phase=%s Round=%d"),
        CountConnectedHumanPlayers(),
        RequiredPlayerCount,
        bGameStarted ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound);
}

void AMainGameMode::BeginPhase(EGamePhase CurrPhase)
{
    if (StrategyMap.Contains(CurrPhase))
    {
        CurrentStrategy = StrategyMap[CurrPhase];
    }
    else
    {
        CurrentStrategy = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main BeginPhase failed. Missing strategy phase=%d Round=%d ServerPhase=%s"),
            static_cast<int32>(CurrPhase),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main BeginPhase phase=%d strategy=%s Round=%d ServerPhase=%s"),
        static_cast<int32>(CurrPhase),
        CurrentStrategy ? *CurrentStrategy->GetName() : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    if (CurrentStrategy)
    {
        CurrentStrategy->OnPhaseStart();
    }
}

void AMainGameMode::EndPhase()
{
    if (CurrentStrategy)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main EndPhase strategy=%s Round=%d ServerPhase=%s"),
            *CurrentStrategy->GetName(),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        CurrentStrategy->OnPhaseEnd();
        CurrentStrategy = nullptr;
    }
}

void AMainGameMode::ChangePhase(EGamePhase NewPhase)
{
    UE_LOG(LogTemp, Warning, TEXT("[DS] Main ChangePhase ignored newPhase=%d Round=%d CurrentServerPhase=%s GameEnd=%d"),
        static_cast<int32>(NewPhase),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        bGameEndReached ? 1 : 0);
}

void AMainGameMode::BroadcastSwitchMode(EGamePhase NewPhase)
{
    int32 TargetCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(It->Get()))
        {
            PC->SwitchMode(NewPhase);
            TargetCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main BroadcastSwitchMode phase=%d targets=%d Round=%d ServerPhase=%s"),
        static_cast<int32>(NewPhase),
        TargetCount,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::BroadcastSwitchLevel(FName LevelToUnload, FName LevelToLoad)
{
    int32 TargetCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(It->Get()))
        {
            PC->SwitchToLevel(LevelToUnload, LevelToLoad);
            TargetCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main BroadcastSwitchLevel unload=%s load=%s targets=%d Round=%d ServerPhase=%s"),
        *LevelToUnload.ToString(),
        *LevelToLoad.ToString(),
        TargetCount,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

bool AMainGameMode::IsBattleRoyalePhase() const
{
    return bGameStarted && CurrentServerPhase == EDediServerPhase::BattleRoyale;
}

void AMainGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
    if (!bGameStarted || CurrentServerPhase == EDediServerPhase::Ready || CurrentServerPhase == EDediServerPhase::TransitionToCard || CurrentServerPhase == EDediServerPhase::Result || CurrentServerPhase == EDediServerPhase::TransitionToBattle || CurrentServerPhase == EDediServerPhase::GameEnd)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main Ignore action action=%s Round=%d Phase=%s"),
            *ActionName.ToString(),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    if (CurrentStrategy)
    {
        CurrentStrategy->OnPlayerAction(Executor, ActionName);
    }
}

void AMainGameMode::InitStrategy()
{
    StrategyMap.Empty();

    for (auto& Pair : StrategyClassMap)
    {
        if (!Pair.Value)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Main InitStrategy skipped null phase=%d"), static_cast<int32>(Pair.Key));
            continue;
        }

        UPhaseStrategy* Strategy = NewObject<UPhaseStrategy>(this, Pair.Value);
        if (Strategy)
        {
            Strategy->Initialize(this);
            StrategyMap.Add(Pair.Key, Strategy);

            UE_LOG(LogTemp, Warning, TEXT("[DS] Main InitStrategy phase=%d strategy=%s"),
                static_cast<int32>(Pair.Key),
                *Strategy->GetName());
        }
    }
}

void AMainGameMode::TryStartGameIfReady()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main TryStart ignored. GameEnd reached RoomId=%d Round=%d"), DediRoomId, CurrentRound);
        return;
    }

    if (bGameStarted)
    {
        return;
    }

    const int32 HumanPlayers = CountConnectedHumanPlayers();
    if (HumanPlayers < RequiredPlayerCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main WaitingPlayers HumanPlayers=%d/%d"),
            HumanPlayers,
            RequiredPlayerCount);
        return;
    }

    bGameStarted = true;
    CurrentRound = 1;

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main RequiredPlayersReady HumanPlayers=%d/%d StartRound=%d MaxRound=%d"),
        HumanPlayers,
        RequiredPlayerCount,
        CurrentRound,
        MaxRoundCount);

    StartReadyPhase();
}

int32 AMainGameMode::CountConnectedHumanPlayers() const
{
    if (!GetWorld())
    {
        return 0;
    }

    int32 Count = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (It->Get())
        {
            Count++;
        }
    }

    return Count;
}

void AMainGameMode::StartReadyPhase()
{
    EndPhase();
    StartTimedServerPhase(EDediServerPhase::Ready, GetReadyDuration());
}

void AMainGameMode::StartBattleRoyalePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartBattleRoyale after GameEnd Round=%d"), CurrentRound);
        return;
    }

    const bool bTPSAlreadyLoadedByTransition = CurrentServerPhase == EDediServerPhase::TransitionToBattle;
    if (!bTPSAlreadyLoadedByTransition)
    {
        BroadcastSwitchLevel(NAME_None, TEXT("TPS_Game_Stage"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main SkipDuplicateTPSLoad Round=%d ServerPhase=%s"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
    }

    BroadcastSwitchMode(EGamePhase::TPS);
    SpawnRoundCardBundleForBattleRoyale();
    SetPlayerPawnGameplayEnabled(true, TEXT("BattleRoyale"));
    StartTimedServerPhase(EDediServerPhase::BattleRoyale, GetBattleRoyaleDuration());
    BeginPhase(EGamePhase::TPS);
}

void AMainGameMode::StartTransitionToCardPhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTransitionToCard after GameEnd Round=%d"), CurrentRound);
        return;
    }

    EndPhase();
    SetPlayerPawnGameplayEnabled(false, TEXT("TransitionToCard"));
    ClearCardDrops();
    ClearPlayerPawnMovementBases(TEXT("TransitionToCard"));
    BroadcastSwitchLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));
    StartTimedServerPhase(EDediServerPhase::TransitionToCard, GetTransitionDuration());
}

void AMainGameMode::StartCardGamePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartCardGame after GameEnd Round=%d"), CurrentRound);
        return;
    }

    ClearPlayerPawnMovementBases(TEXT("CardGame"));
    EnsureThreeCardsForCardGame();
    ResetSeotdaRoundStates();
    BroadcastSwitchMode(EGamePhase::Card);
    ClearServerPhaseTimer();
    CurrentServerPhase = EDediServerPhase::CardGame;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseStart Round=%d Phase=%s Duration=0 ManualCardGame=1"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
    BeginPhase(EGamePhase::Card);
}

void AMainGameMode::StartResultPhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartResult after GameEnd Round=%d"), CurrentRound);
        return;
    }

    EndPhase();

    if (!bSeotdaRoundResolved && SeotdaRoundStates.Num() > 0)
    {
        ResolveSeotdaRoundResult(TEXT("ResultPhaseFallback"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] RoundResult Round=%d Summary=%s"),
        CurrentRound,
        *LastSeotdaRoundResultSummary);

    StartTimedServerPhase(EDediServerPhase::Result, GetResultDuration());
}

void AMainGameMode::StartTransitionToBattlePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTransitionToBattle after GameEnd Round=%d"), CurrentRound);
        return;
    }

    ClearCardDrops();
    ClearRoundCardsForAllPlayers();
    ClearPlayerPawnMovementBases(TEXT("TransitionToBattle"));
    BroadcastSwitchLevel(TEXT("Card_Game_Stage"), TEXT("TPS_Game_Stage"));
    StartTimedServerPhase(EDediServerPhase::TransitionToBattle, GetTransitionDuration());
}

void AMainGameMode::StartGameEndPhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] GameEnd ignored duplicate RoomId=%d Round=%d"), DediRoomId, CurrentRound);
        return;
    }

    bGameEndReached = true;
    bGameStarted = false;

    EndPhase();
    ClearCardDrops();
    ClearRoundCardsForAllPlayers();
    ClearServerPhaseTimer();

    CurrentServerPhase = EDediServerPhase::GameEnd;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    SetPlayerPawnGameplayEnabled(false, TEXT("GameEnd"));

    FString WinnerName = TEXT("None");
    FString MoneySummary = TEXT("None");

    int32 BestMoney = MIN_int32;
    int32 BestMoneyPlayerCount = 0;

    TArray<FString> MoneyParts;

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
            if (!MPC)
            {
                continue;
            }

            AMainPlayerState* PS = MPC->GetPlayerState<AMainPlayerState>();
            if (!PS)
            {
                continue;
            }

            const int32 Money = GetSeotdaPlayerMoney(PS);
            const FString PlayerName = PS->GetPlayerName();

            MoneyParts.Add(FString::Printf(TEXT("%s=%d"), *PlayerName, Money));

            if (Money > BestMoney)
            {
                BestMoney = Money;
                BestMoneyPlayerCount = 1;
                WinnerName = PlayerName;
            }
            else if (Money == BestMoney)
            {
                BestMoneyPlayerCount++;
            }
        }
    }

    if (MoneyParts.Num() > 0)
    {
        MoneySummary = FString::Join(MoneyParts, TEXT(", "));
    }

    if (BestMoneyPlayerCount >= 2)
    {
        WinnerName = FString::Printf(TEXT("Tie(%d players, Money=%d)"), BestMoneyPlayerCount, BestMoney);
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] GameEnd RoomId=%d Round=%d MaxRound=%d Winner=%s MoneySummary=%s"),
        DediRoomId,
        CurrentRound,
        MaxRoundCount,
        *WinnerName,
        *MoneySummary);

    NotifyIocpMatchEnd(WinnerName, MoneySummary);

    const FString FinalResultText = FString::Printf(
        TEXT("[MATCH END]\nWinner=%s\nRound=%d/%d\nMoney=%s"),
        *WinnerName,
        CurrentRound,
        MaxRoundCount,
        *MoneySummary
    );

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
            if (MPC)
            {
                MPC->Client_ShowSeotdaResult(FinalResultText);
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            MatchEndShutdownTimerHandle,
            this,
            &AMainGameMode::ShutdownDedicatedServerAfterMatchEnd,
            10.0f,
            false
        );

        UE_LOG(LogTemp, Warning, TEXT("[DS] MatchEndShutdownScheduled Delay=10.0 RoomId=%d Round=%d"),
            DediRoomId,
            CurrentRound);
    }
}


FString AMainGameMode::GetOwnedCardsDebugString(const AMainPlayerState* PS) const
{
    if (!PS)
    {
        return TEXT("None");
    }

    TArray<FString> Parts;

    for (const FOwnedCardInfo& CardInfo : PS->OwnedCards)
    {
        Parts.Add(FString::Printf(
            TEXT("#%d:%s"),
            CardInfo.CardInstanceId,
            *CardDebug::ToString(CardInfo.CardID)
        ));
    }

    return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("Empty");
}

bool AMainGameMode::TryPickupCard(AMainPlayerController* RequestingPC, ACardDropActor* TargetCard)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!RequestingPC || !TargetCard)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=InvalidRequest"));
        return false;
    }

    if (!IsCardPickupAllowed())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            GetServerPhaseName(CurrentServerPhase));
        return false;
    }

    if (TargetCard->IsPickedUp())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=AlreadyPicked Player=%s Instance=%d"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId());
        return false;
    }

    APawn* Pawn = RequestingPC->GetPawn();
    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!Pawn || !PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=MissingPawnOrPS Player=%s"), *RequestingPC->GetName());
        return false;
    }

    if (PS->OwnedCards.Num() >= MaxCardsPerPlayerPerRound)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=CardLimit Player=%s Owned=%d Max=%d OwnedCards=[%s]"),
            *RequestingPC->GetName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound,
            *GetOwnedCardsDebugString(PS));

        return false;
    }

    const float Distance = FVector::Dist(Pawn->GetActorLocation(), TargetCard->GetActorLocation());
    if (Distance > CardPickupRange)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=Distance Player=%s Instance=%d Distance=%.2f Range=%.2f"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId(),
            Distance,
            CardPickupRange);
        return false;
    }

    FServerCardRecord* Record = ServerCardRecords.Find(TargetCard->GetCardInstanceId());
    if (!Record)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=NoRecord Player=%s Instance=%d"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId());
        return false;
    }

    if (Record->State != ECardRuntimeState::WorldDrop || Record->DropActor.Get() != TargetCard)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=StateMismatch Player=%s Instance=%d State=%d"),
            *RequestingPC->GetName(),
            Record->CardInstanceId,
            static_cast<int32>(Record->State));
        return false;
    }

    FOwnedCardInfo CardInfo;
    CardInfo.CardInstanceId = Record->CardInstanceId;
    CardInfo.CardID = Record->CardID;

    PS->AddOwnedCard(CardInfo);

    Record->State = ECardRuntimeState::Owned;
    Record->OwnerPlayerState = PS;
    Record->DropActor = nullptr;

    TargetCard->MarkPickedUp();
    ActiveCardDrops.RemoveAll([TargetCard](const TObjectPtr<ACardDropActor>& CardActor)
    {
        return CardActor.Get() == TargetCard;
    });
    TargetCard->Destroy();

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupOK Player=%s Instance=%d Card=%d Name=%s OwnedCount=%d OwnedCards=[%s]"),
        *PS->GetPlayerName(),
        CardInfo.CardInstanceId,
        static_cast<int32>(CardInfo.CardID),
        *CardDebug::ToString(CardInfo.CardID),
        PS->PublicCardCount,
        *GetOwnedCardsDebugString(PS));


    return true;
}



bool AMainGameMode::SubmitSeotdaSelection(AMainPlayerController* RequestingPC, bool bCard0, bool bCard1, bool bCard2)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!RequestingPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=InvalidRequest"));
        return false;
    }

    if (CurrentServerPhase != EDediServerPhase::CardGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            GetServerPhaseName(CurrentServerPhase));
        return false;
    }

    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=MissingPS Player=%s"), *RequestingPC->GetName());
        return false;
    }

    if (PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=InvalidCardCount Player=%s Count=%d Required=%d"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound);
        return false;
    }

    const int32 SelectedCount = (bCard0 ? 1 : 0) + (bCard1 ? 1 : 0) + (bCard2 ? 1 : 0);
    if (SelectedCount != 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=InvalidSelectCount Player=%s Count=%d"),
            *PS->GetPlayerName(),
            SelectedCount);
        return false;
    }

    FSeotdaPlayerRoundState* ExistingState = SeotdaRoundStates.Find(PS);
    if (ExistingState && ExistingState->bSubmitted)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitReject Reason=AlreadySubmitted Player=%s"), *PS->GetPlayerName());
        return false;
    }

    TArray<FOwnedCardInfo> SelectedCards;
    if (bCard0)
    {
        SelectedCards.Add(PS->OwnedCards[0]);
    }
    if (bCard1)
    {
        SelectedCards.Add(PS->OwnedCards[1]);
    }
    if (bCard2)
    {
        SelectedCards.Add(PS->OwnedCards[2]);
    }

    FSeotdaHandResult HandResult = EvaluateSeotdaHand(SelectedCards[0], SelectedCards[1]);

    FSeotdaPlayerRoundState NewState;
    NewState.PlayerState = PS;
    NewState.bSubmitted = true;

    BroadcastSeotdaState();

    NewState.HandResult = HandResult;
    NewState.SelectedCardInstanceIds = HandResult.UsedCardInstanceIds;
    SeotdaRoundStates.Add(PS, NewState);

    for (int32 InstanceId : HandResult.UsedCardInstanceIds)
    {
        if (FServerCardRecord* Record = ServerCardRecords.Find(InstanceId))
        {
            if (Record->OwnerPlayerState.Get() == PS)
            {
                Record->State = ECardRuntimeState::Used;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitOK Player=%s Selected=[#%d:%s, #%d:%s] Combo=%s Rank=%d SubRank=%d AllCards=[%s]"),
        *PS->GetPlayerName(),

        SelectedCards[0].CardInstanceId,
        *CardDebug::ToString(SelectedCards[0].CardID),

        SelectedCards[1].CardInstanceId,
        *CardDebug::ToString(SelectedCards[1].CardID),

        *HandResult.Name,
        HandResult.Rank,
        HandResult.SubRank,

        *GetOwnedCardsDebugString(PS));

    BroadcastSeotdaState();

    TryResolveSeotdaRoundIfReady();
    return true;
}

void AMainGameMode::ResetSeotdaRoundStates()
{
    SeotdaRoundStates.Empty();
    SeotdaTurnOrder.Empty();
    SeotdaPot = 0;
    SeotdaCurrentBet = 0;
    SeotdaCurrentTurnIndex = 0;
    bSeotdaBettingActive = false;
    bSeotdaRoundResolved = false;
    LastSeotdaRoundResultSummary = TEXT("Pending");

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda Reset Round=%d"), CurrentRound);
}

void AMainGameMode::TryResolveSeotdaRoundIfReady()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    int32 TargetCount = 0;
    int32 SubmittedCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
        if (!PS)
        {
            continue;
        }

        TargetCount++;
        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (State && State->bSubmitted)
        {
            SubmittedCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda SubmitProgress submitted=%d targets=%d Round=%d"), SubmittedCount, TargetCount, CurrentRound);

    if (TargetCount <= 0 || SubmittedCount < TargetCount || bSeotdaBettingActive)
    {
        return;
    }

    StartSeotdaBettingRound();
    BroadcastSeotdaState();

}

void AMainGameMode::StartSeotdaBettingRound()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    SeotdaTurnOrder.Empty();
    SeotdaPot = 0;
    SeotdaCurrentBet = 0;
    SeotdaCurrentTurnIndex = 0;

    for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
{
FSeotdaPlayerRoundState& State = Pair.Value;

if (!State.PlayerState.IsValid())
{
continue;
}

if (!State.bSubmitted)
{
continue;
}

State.bActedThisBetRound = false;
State.BetMoney = 0;
}

bSeotdaBettingActive = true;

    ClearServerPhaseTimer();
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BettingTimerDisabled Round=%d Pot=%d CurrentBet=%d"),
        CurrentRound,
        SeotdaPot,
        SeotdaCurrentBet);


    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
        if (!PS)
        {
            continue;
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State || !State->bSubmitted)
        {
            continue;
        }

        State->bFolded = false;
        State->bActedThisBetRound = false;
        State->BetMoney = 0;
        SeotdaTurnOrder.Add(PS);
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BettingStart players=%d pot=%d currentBet=%d round=%d"),
        SeotdaTurnOrder.Num(),
        SeotdaPot,
        SeotdaCurrentBet,
        CurrentRound);

    if (SeotdaTurnOrder.Num() <= 1)
    {
        ResolveSeotdaRoundResult(TEXT("SinglePlayer"));
        return;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetTurn Player=%s Index=%d Pot=%d CurrentBet=%d"),
        TurnPS ? *TurnPS->GetPlayerName() : TEXT("<NULL>"),
        SeotdaCurrentTurnIndex,
        SeotdaPot,
        SeotdaCurrentBet);

    BroadcastSeotdaState();

}

bool AMainGameMode::SubmitSeotdaBetAction(AMainPlayerController* RequestingPC, EBettingAction Action)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!RequestingPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=InvalidRequest Action=%d"), static_cast<int32>(Action));
        return false;
    }

    if (CurrentServerPhase != EDediServerPhase::CardGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=InvalidPhase Player=%s Phase=%s Action=%d"),
            *RequestingPC->GetName(),
            GetServerPhaseName(CurrentServerPhase),
            static_cast<int32>(Action));
        return false;
    }

    if (!bSeotdaBettingActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=BettingNotActive Player=%s Action=%d"),
            *RequestingPC->GetName(),
            static_cast<int32>(Action));
        return false;
    }

    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=MissingPS Player=%s Action=%d"),
            *RequestingPC->GetName(),
            static_cast<int32>(Action));
        return false;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    if (TurnPS != PS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=NotYourTurn Player=%s Turn=%s Action=%d"),
            *PS->GetPlayerName(),
            TurnPS ? *TurnPS->GetPlayerName() : TEXT("<NULL>"),
            static_cast<int32>(Action));
        return false;
    }

    FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
    if (!State || !State->bSubmitted || State->bFolded)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=InvalidState Player=%s Action=%d"),
            *PS->GetPlayerName(),
            static_cast<int32>(Action));
        return false;
    }

    const int32 OldCurrentBet = SeotdaCurrentBet;
    const int32 CallAmount = FMath::Max(0, SeotdaCurrentBet - State->BetMoney);
    int32 RequestedPay = 0;
    bool bFoldAction = false;

    switch (Action)
    {
    case EBettingAction::Check:
        if (CallAmount > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=CheckNeedsCall Player=%s Call=%d"), *PS->GetPlayerName(), CallAmount);
            return false;
        }
        RequestedPay = 0;
        break;
    case EBettingAction::Call:
        RequestedPay = CallAmount;
        break;
    case EBettingAction::Half:
        RequestedPay = CallAmount + FMath::Max(1, (SeotdaPot + CallAmount) / 2);
        break;
    case EBettingAction::AllIn:
        RequestedPay = GetSeotdaPlayerMoney(PS);
        break;
    case EBettingAction::Die:
        bFoldAction = true;
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetReject Reason=InvalidAction Player=%s Action=%d"),
            *PS->GetPlayerName(),
            static_cast<int32>(Action));
        return false;
    }

    int32 Paid = 0;
    if (bFoldAction)
    {
        State->bFolded = true;
        State->bActedThisBetRound = true;
    }
    else
    {
        Paid = PaySeotdaBet(PS, RequestedPay);
        State->BetMoney += Paid;
        if (State->BetMoney > SeotdaCurrentBet)
        {
            SeotdaCurrentBet = State->BetMoney;
        }

        const bool bRaised = SeotdaCurrentBet > OldCurrentBet;
        if (bRaised)
        {
            for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
            {
                if (Pair.Key != PS && Pair.Value.bSubmitted && !Pair.Value.bFolded)
                {
                    Pair.Value.bActedThisBetRound = false;
                }
            }
        }

        State->bActedThisBetRound = true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetOK Player=%s Action=%d Paid=%d BetMoney=%d Pot=%d CurrentBet=%d Money=%d Folded=%d"),
        *PS->GetPlayerName(),
        static_cast<int32>(Action),
        Paid,
        State->BetMoney,
        SeotdaPot,
        SeotdaCurrentBet,
        GetSeotdaPlayerMoney(PS),
        State->bFolded ? 1 : 0);

    if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
    {
        ResolveSeotdaRoundResult(TEXT("BetSettled"));
        BroadcastSeotdaState();

        if (CurrentServerPhase == EDediServerPhase::CardGame)
        {
            FinishCurrentServerPhase(TEXT("SeotdaBetSettled"));
        }

        return true;
    }

    AdvanceSeotdaBettingTurn();
    return true;
}

void AMainGameMode::AdvanceSeotdaBettingTurn()
{
    if (SeotdaTurnOrder.Num() <= 0)
    {
        ResolveSeotdaRoundResult(TEXT("NoTurnOrder"));
        return;
    }

    for (int32 Step = 0; Step < SeotdaTurnOrder.Num(); ++Step)
    {
        SeotdaCurrentTurnIndex = (SeotdaCurrentTurnIndex + 1) % SeotdaTurnOrder.Num();
        AMainPlayerState* CandidatePS = SeotdaTurnOrder[SeotdaCurrentTurnIndex].Get();
        FSeotdaPlayerRoundState* State = CandidatePS ? SeotdaRoundStates.Find(CandidatePS) : nullptr;
        if (CandidatePS && State && State->bSubmitted && !State->bFolded)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda BetTurn Player=%s Index=%d Pot=%d CurrentBet=%d NeedCall=%d"),
                *CandidatePS->GetPlayerName(),
                SeotdaCurrentTurnIndex,
                SeotdaPot,
                SeotdaCurrentBet,
                FMath::Max(0, SeotdaCurrentBet - State->BetMoney));

    BroadcastSeotdaState();

            return;
        }
    }

    ResolveSeotdaRoundResult(TEXT("NoActiveTurn"));
}

void AMainGameMode::ResolveSeotdaRoundResult(const TCHAR* Reason)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bSeotdaRoundResolved)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda ResultSkip AlreadyResolved Summary=%s"),
            *LastSeotdaRoundResultSummary);
        return;
    }

    for (int32 RedealAttempt = 0; RedealAttempt < 8 && ShouldForceSeotdaRedeal(); ++RedealAttempt)
    {
        if (!TryApplySeotdaRedealFromRemainingCards(Reason))
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealStop Reason=NotEnoughRemainingCards Attempt=%d"), RedealAttempt);
            break;
        }
    }

    FSeotdaPlayerRoundState* BestState = nullptr;
    bool bTie = false;

    for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        FSeotdaPlayerRoundState& State = Pair.Value;

        if (!State.bSubmitted || State.bFolded)
        {
            continue;
        }

        if (!BestState)
        {
            BestState = &State;
            bTie = false;
            continue;
        }

        const int32 CompareResult = CompareSeotdaHands(State.HandResult, BestState->HandResult);

        if (CompareResult > 0)
        {
            BestState = &State;
            bTie = false;
        }
        else if (CompareResult == 0)
        {
            bTie = true;
        }
    }

    if (!BestState || !BestState->PlayerState.IsValid())
    {
        LastSeotdaRoundResultSummary = FString::Printf(
            TEXT("Winner=None Combo=None Pot=%d Reason=%s"),
            SeotdaPot,
            Reason ? Reason : TEXT("<NULL>")
        );

        bSeotdaBettingActive = false;
        bSeotdaRoundResolved = true;
        BroadcastSeotdaState();

        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda ResultFailed %s"), *LastSeotdaRoundResultSummary);
        return;
    }

    AMainPlayerState* WinnerPS = BestState->PlayerState.Get();

    if (WinnerPS && SeotdaPot > 0)
    {
        WinnerPS->AddGold(SeotdaPot);
    }

    LastSeotdaRoundResultSummary = FString::Printf(
        TEXT("Winner=%s Combo=%s Rank=%d SubRank=%d Pot=%d Tie=%d Reason=%s Money=%d"),

        WinnerPS ? *WinnerPS->GetPlayerName() : TEXT("<NULL>"),
        *BestState->HandResult.Name,
        BestState->HandResult.Rank,
        BestState->HandResult.SubRank,
        SeotdaPot,
        bTie ? 1 : 0,
        Reason ? Reason : TEXT("<NULL>"),
        WinnerPS ? GetSeotdaPlayerMoney(WinnerPS) : 0
    );

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda Winner %s"), *LastSeotdaRoundResultSummary);

    bSeotdaBettingActive = false;
    bSeotdaRoundResolved = true;
    BroadcastSeotdaState();

    const FString ClientResultText = FString::Printf(
        TEXT("[ROUND %d RESULT] %s"),
        CurrentRound,
        *LastSeotdaRoundResultSummary
    );

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
            if (MPC)
            {
                MPC->Client_ShowSeotdaResult(ClientResultText);
            }
        }
    }
}
void AMainGameMode::BroadcastSeotdaState() const
{
if (!HasAuthority() || !GetWorld())
{
return;
}

AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
const FString TurnName = TurnPS ? TurnPS->GetPlayerName() : TEXT("None");

for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
{
AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
if (!MPC)
{
continue;
}

AMainPlayerState* PS = MPC->GetPlayerState<AMainPlayerState>();

bool bMySubmitted = false;
bool bMyFolded = false;
int32 MyBetMoney = 0;
int32 NeedCall = 0;
bool bMyTurn = false;

if (PS)
{
bMyTurn = (TurnPS == PS);

if (const FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS))
{
bMySubmitted = State->bSubmitted;
bMyFolded = State->bFolded;
MyBetMoney = State->BetMoney;
NeedCall = FMath::Max(0, SeotdaCurrentBet - MyBetMoney);
}
}

MPC->Client_UpdateSeotdaState(
CurrentRound,
bSeotdaBettingActive,
TurnName,
SeotdaPot,
SeotdaCurrentBet,
MyBetMoney,
NeedCall,
bMyTurn,
bMySubmitted,
bMyFolded,
bSeotdaRoundResolved
);
}
}
AMainPlayerState* AMainGameMode::GetCurrentSeotdaTurnPlayer() const
{
    if (SeotdaTurnOrder.Num() <= 0 || !SeotdaTurnOrder.IsValidIndex(SeotdaCurrentTurnIndex))
    {
        return nullptr;
    }

    return SeotdaTurnOrder[SeotdaCurrentTurnIndex].Get();
}

int32 AMainGameMode::GetSeotdaPlayerMoney(const AMainPlayerState* TargetPS) const
{
    return TargetPS ? TargetPS->CurPlayerData.HoldingGold : 0;
}

int32 AMainGameMode::PaySeotdaBet(AMainPlayerState* TargetPS, int32 Amount)
{
    if (!TargetPS || Amount <= 0)
    {
        return 0;
    }

    const int32 ActualPay = FMath::Min(Amount, FMath::Max(0, TargetPS->CurPlayerData.HoldingGold));
    if (ActualPay > 0)
    {
        TargetPS->AddGold(-ActualPay);
        SeotdaPot += ActualPay;
    }

    return ActualPay;
}

int32 AMainGameMode::GetActiveSeotdaPlayerCount() const
{
    int32 Count = 0;
    for (const TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        if (Pair.Value.bSubmitted && !Pair.Value.bFolded)
        {
            Count++;
        }
    }
    return Count;
}

bool AMainGameMode::AreSeotdaBetsSettled() const
{
int32 ActiveSubmittedCount = 0;
int32 ActedCount = 0;

for (const TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
{
const FSeotdaPlayerRoundState& State = Pair.Value;

if (!State.PlayerState.IsValid())
{
continue;
}

if (!State.bSubmitted)
{
continue;
}

if (State.bFolded)
{
continue;
}

ActiveSubmittedCount++;

if (!State.bActedThisBetRound)
{
UE_LOG(LogTemp, Warning,
TEXT("[DS] Seotda SettleCheck Result=0 Reason=NotActed Player=%s Active=%d Acted=%d CurrentBet=%d PlayerBet=%d"),
*State.PlayerState->GetPlayerName(),
ActiveSubmittedCount,
ActedCount,
SeotdaCurrentBet,
State.BetMoney
);

return false;
}

if (State.BetMoney < SeotdaCurrentBet)
{
UE_LOG(LogTemp, Warning,
TEXT("[DS] Seotda SettleCheck Result=0 Reason=NeedCall Player=%s Active=%d Acted=%d CurrentBet=%d PlayerBet=%d"),
*State.PlayerState->GetPlayerName(),
ActiveSubmittedCount,
ActedCount,
SeotdaCurrentBet,
State.BetMoney
);

return false;
}

ActedCount++;
}

const bool bSettled = ActiveSubmittedCount >= 2 && ActedCount == ActiveSubmittedCount;

UE_LOG(LogTemp, Warning,
TEXT("[DS] Seotda SettleCheck Result=%d Active=%d Acted=%d CurrentBet=%d"),
bSettled ? 1 : 0,
ActiveSubmittedCount,
ActedCount,
SeotdaCurrentBet
);

return bSettled;
}

AMainGameMode::FSeotdaHandResult AMainGameMode::EvaluateSeotdaHand(const FOwnedCardInfo& FirstCard, const FOwnedCardInfo& SecondCard) const
{
    FSeotdaHandResult Result;
    Result.UsedCardInstanceIds.Add(FirstCard.CardInstanceId);
    Result.UsedCardInstanceIds.Add(SecondCard.CardInstanceId);

    const ECardID FirstCardID = FirstCard.CardID;
    const ECardID SecondCardID = SecondCard.CardID;

    const int32 FirstMonth = GetSeotdaCardMonth(FirstCardID);
    const int32 SecondMonth = GetSeotdaCardMonth(SecondCardID);

    if (FirstMonth <= 0 || SecondMonth <= 0)
    {
        Result.Rank = -1;
        Result.SubRank = 0;
        Result.Name = TEXT("Invalid");
        return Result;
    }

    const bool bFirstGwang = IsSeotdaGwang(FirstCardID);
    const bool bSecondGwang = IsSeotdaGwang(SecondCardID);
    const bool bBothGwang = bFirstGwang && bSecondGwang;

    auto IsYulCard = [](ECardID CardID) -> bool
    {
        switch (CardID)
        {
        case ECardID::Feb_Yul:
        case ECardID::Apr_Yul:
        case ECardID::May_Yul:
        case ECardID::Jun_Yul:
        case ECardID::Jul_Yul:
        case ECardID::Aug_Yul:
        case ECardID::Sep_Yul:
        case ECardID::Oct_Yul:
            return true;
        default:
            return false;
        }
    };

    const bool bBothYul = IsYulCard(FirstCardID) && IsYulCard(SecondCardID);

    if (bBothGwang && HasSeotdaMonths(FirstMonth, SecondMonth, 3, 8))
    {
        Result.Rank = 12000;
        Result.SubRank = 38;
        Result.Name = TEXT("SamPalGwangDdang");
        return Result;
    }

    if (bBothGwang)
    {
        Result.Rank = 11000;
        Result.SubRank = FirstMonth + SecondMonth;
        Result.Name = TEXT("GwangDdang");
        return Result;
    }

    if (FirstMonth == SecondMonth)
    {
        Result.Rank = 10000 + FirstMonth;
        Result.SubRank = FirstMonth;
        Result.Name = FString::Printf(TEXT("%dDdang"), FirstMonth);
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 3, 7))
    {
        Result.Rank = 500;
        Result.SubRank = 37;
        Result.Name = TEXT("TtaengJabi");
        Result.SpecialRule = ESeotdaSpecialRule::TtaengJabi;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 7))
    {
        Result.Rank = 500;
        Result.SubRank = 47;
        Result.Name = TEXT("AmhaengEosa");
        Result.SpecialRule = ESeotdaSpecialRule::AmhaengEosa;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 9))
    {
        Result.Rank = 400;
        Result.SubRank = 49;
        Result.Name = bBothYul ? TEXT("MeongteongguriGusa") : TEXT("Gusa");
        Result.SpecialRule = bBothYul ? ESeotdaSpecialRule::MeongteongguriGusa : ESeotdaSpecialRule::Gusa;
        Result.bForcesRedeal = true;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 2))
    {
        Result.Rank = 9000;
        Result.SubRank = 12;
        Result.Name = TEXT("Ali");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 4))
    {
        Result.Rank = 8000;
        Result.SubRank = 14;
        Result.Name = TEXT("Doksa");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 9))
    {
        Result.Rank = 7000;
        Result.SubRank = 19;
        Result.Name = TEXT("Guping");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 10))
    {
        Result.Rank = 6000;
        Result.SubRank = 110;
        Result.Name = TEXT("Jangping");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 10))
    {
        Result.Rank = 5000;
        Result.SubRank = 410;
        Result.Name = TEXT("Jangsa");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 6))
    {
        Result.Rank = 4000;
        Result.SubRank = 46;
        Result.Name = TEXT("Seryuk");
        return Result;
    }

    const int32 Gut = (FirstMonth + SecondMonth) % 10;

    if (Gut == 9)
    {
        Result.Rank = 3000;
        Result.SubRank = 9;
        Result.Name = TEXT("GapOh");
        return Result;
    }

    if (Gut == 0)
    {
        Result.Rank = 0;
        Result.SubRank = 0;
        Result.Name = TEXT("Mangtong");
        return Result;
    }

    Result.Rank = 1000 + Gut;
    Result.SubRank = Gut;
    Result.Name = FString::Printf(TEXT("%dGut"), Gut);
    return Result;
}

int32 AMainGameMode::GetSeotdaCardMonth(ECardID CardID) const
{
    switch (CardID)
    {
    case ECardID::Jan_Gwang:
    case ECardID::Jan_HongDdi:
        return 1;

    case ECardID::Feb_Yul:
    case ECardID::Feb_HongDdi:
        return 2;

    case ECardID::Mar_Gwang:
    case ECardID::Mar_HongDdi:
        return 3;

    case ECardID::Apr_Yul:
    case ECardID::Apr_ChoDdi:
        return 4;

    case ECardID::May_Yul:
    case ECardID::May_ChoDdi:
        return 5;

    case ECardID::Jun_Yul:
    case ECardID::Jun_CheongDdi:
        return 6;

    case ECardID::Jul_Yul:
    case ECardID::Jul_ChoDdi:
        return 7;

    case ECardID::Aug_Gwang:
    case ECardID::Aug_Yul:
        return 8;

    case ECardID::Sep_Yul:
    case ECardID::Sep_CheongDdi:
        return 9;

    case ECardID::Oct_Yul:
    case ECardID::Oct_CheongDdi:
        return 10;

    default:
        return 0;
    }
}

bool AMainGameMode::IsSeotdaGwang(ECardID CardID) const
{
    return CardID == ECardID::Jan_Gwang ||
        CardID == ECardID::Mar_Gwang ||
        CardID == ECardID::Aug_Gwang;
}

bool AMainGameMode::HasSeotdaMonths(int32 FirstMonth, int32 SecondMonth, int32 A, int32 B) const
{
    return (FirstMonth == A && SecondMonth == B) || (FirstMonth == B && SecondMonth == A);
}

bool AMainGameMode::TryPickupNearestCard(AMainPlayerController* RequestingPC)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!RequestingPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearestReject Reason=InvalidRequest"));
        return false;
    }

    if (!IsCardPickupAllowed())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearestReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            GetServerPhaseName(CurrentServerPhase));
        return false;
    }

    APawn* Pawn = RequestingPC->GetPawn();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearestReject Reason=MissingPawn Player=%s"), *RequestingPC->GetName());
        return false;
    }

    const FVector PawnLocation = Pawn->GetActorLocation();
    const float MaxDistanceSq = FMath::Square(CardPickupRange);
    float BestDistanceSq = MaxDistanceSq;
    ACardDropActor* BestCard = nullptr;

    for (TObjectPtr<ACardDropActor> CardActorPtr : ActiveCardDrops)
    {
        ACardDropActor* CardActor = CardActorPtr.Get();
        if (!IsValid(CardActor) || CardActor->IsPickedUp())
        {
            continue;
        }

        FServerCardRecord* Record = ServerCardRecords.Find(CardActor->GetCardInstanceId());
        if (!Record || Record->State != ECardRuntimeState::WorldDrop || Record->DropActor.Get() != CardActor)
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared(PawnLocation, CardActor->GetActorLocation());
        if (DistanceSq <= BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestCard = CardActor;
        }
    }

    if (!BestCard)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearestReject Reason=NoNearbyCard Player=%s Range=%.2f"),
            *RequestingPC->GetName(),
            CardPickupRange);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearest Player=%s Instance=%d Card=%d Name=%s Distance=%.2f"),
        *RequestingPC->GetName(),
        BestCard->GetCardInstanceId(),
        static_cast<int32>(BestCard->GetCardID()),
        *CardDebug::ToString(BestCard->GetCardID()),
        FMath::Sqrt(BestDistanceSq));

    return TryPickupCard(RequestingPC, BestCard);
}

TArray<ECardID> AMainGameMode::BuildCardBundleIDs() const
{
    TArray<ECardID> CardIDs;
    CardIDs.Reserve(20);

    // 20-card Seotda deck.
    // 1월: 광, 홍띠
    CardIDs.Add(ECardID::Jan_Gwang);
    CardIDs.Add(ECardID::Jan_HongDdi);

    // 2월: 10끗, 홍띠
    CardIDs.Add(ECardID::Feb_Yul);
    CardIDs.Add(ECardID::Feb_HongDdi);

    // 3월: 광, 홍띠
    CardIDs.Add(ECardID::Mar_Gwang);
    CardIDs.Add(ECardID::Mar_HongDdi);

    // 4월: 10끗, 초띠
    CardIDs.Add(ECardID::Apr_Yul);
    CardIDs.Add(ECardID::Apr_ChoDdi);

    // 5월: 10끗, 초띠
    CardIDs.Add(ECardID::May_Yul);
    CardIDs.Add(ECardID::May_ChoDdi);

    // 6월: 10끗, 청띠
    CardIDs.Add(ECardID::Jun_Yul);
    CardIDs.Add(ECardID::Jun_CheongDdi);

    // 7월: 10끗, 초띠
    CardIDs.Add(ECardID::Jul_Yul);
    CardIDs.Add(ECardID::Jul_ChoDdi);

    // 8월: 광, 10끗
    CardIDs.Add(ECardID::Aug_Gwang);
    CardIDs.Add(ECardID::Aug_Yul);

    // 9월: 10끗, 청띠
    CardIDs.Add(ECardID::Sep_Yul);
    CardIDs.Add(ECardID::Sep_CheongDdi);

    // 10월: 10끗, 청띠
    CardIDs.Add(ECardID::Oct_Yul);
    CardIDs.Add(ECardID::Oct_CheongDdi);

    return CardIDs;
}

void AMainGameMode::ShuffleCardIDs(TArray<ECardID>& CardIDs) const
{
    for (int32 Index = CardIDs.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            CardIDs.Swap(Index, SwapIndex);
        }
    }
}

FVector AMainGameMode::GetDistributedCardDropLocation(int32 Index, int32 TotalCount) const
{
    if (TotalCount <= 0)
    {
        return CardBundleDropCenter;
    }

    const float SafeExtentX = FMath::Max(1.0f, CardBundleDropExtent.X);
    const float SafeExtentY = FMath::Max(1.0f, CardBundleDropExtent.Y);
    const float Aspect = SafeExtentX / SafeExtentY;

    // 기존 방식은 20장일 때 6x4=24칸이 되어 마지막 줄이 치우칠 수 있었다.
    // 이 방식은 20장 기준 5x4에 가깝게 만들어 전체 영역에 더 고르게 배치한다.
    const int32 RowCount = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TotalCount) / FMath::Max(0.25f, Aspect))));
    const int32 ColumnCount = FMath::Max(1, FMath::CeilToInt(static_cast<float>(TotalCount) / static_cast<float>(RowCount)));

    const int32 Row = Index / ColumnCount;
    const int32 Column = Index % ColumnCount;

    const int32 ItemsInThisRow = FMath::Min(ColumnCount, TotalCount - Row * ColumnCount);

    const float FullWidth = CardBundleDropExtent.X * 2.0f;
    const float FullHeight = CardBundleDropExtent.Y * 2.0f;

    const float CellWidth = FullWidth / static_cast<float>(ColumnCount);
    const float CellHeight = FullHeight / static_cast<float>(RowCount);

    const float MinX = CardBundleDropCenter.X - CardBundleDropExtent.X;
    const float MinY = CardBundleDropCenter.Y - CardBundleDropExtent.Y;

    // 마지막 줄이 꽉 차지 않아도 중앙 정렬되게 보정
    const float RowWidth = CellWidth * static_cast<float>(ItemsInThisRow);
    const float RowStartX = CardBundleDropCenter.X - RowWidth * 0.5f;

    const float JitterRatio = FMath::Clamp(CardBundleDropJitterRatio, 0.0f, 0.20f);
    const float JitterX = CellWidth * JitterRatio;
    const float JitterY = CellHeight * JitterRatio;

    const float X = RowStartX + (static_cast<float>(Column) + 0.5f) * CellWidth + FMath::FRandRange(-JitterX, JitterX);
    const float Y = MinY + (static_cast<float>(Row) + 0.5f) * CellHeight + FMath::FRandRange(-JitterY, JitterY);
    const float Z = CardBundleDropCenter.Z;

    return FVector(X, Y, Z);
}

int32 AMainGameMode::GetCardIslandBalanceValue(ECardID CardID) const
{
    switch (CardID)
    {
    case ECardID::Jan_Gwang:
    case ECardID::Mar_Gwang:
    case ECardID::Aug_Gwang:
        return 9;

    case ECardID::Feb_Yul:
    case ECardID::Apr_Yul:
    case ECardID::May_Yul:
    case ECardID::Jun_Yul:
    case ECardID::Jul_Yul:
    case ECardID::Aug_Yul:
    case ECardID::Sep_Yul:
    case ECardID::Oct_Yul:
        return 6;

    case ECardID::Jan_HongDdi:
    case ECardID::Feb_HongDdi:
    case ECardID::Mar_HongDdi:
    case ECardID::Apr_ChoDdi:
    case ECardID::May_ChoDdi:
    case ECardID::Jun_CheongDdi:
    case ECardID::Jul_ChoDdi:
    case ECardID::Sep_CheongDdi:
    case ECardID::Oct_CheongDdi:
        return 5;

    default:
        return 0;
    }
}

int32 AMainGameMode::GetCardIslandGroupBalanceValue(const TArray<ECardID>& CardIDs) const
{
    int32 TotalValue = 0;

    for (ECardID CardID : CardIDs)
    {
        TotalValue += GetCardIslandBalanceValue(CardID);
    }

    return TotalValue;
}

TArray<TArray<ECardID>> AMainGameMode::BuildBalancedIslandCardGroups() const
{
    TArray<TArray<ECardID>> Groups;
    Groups.SetNum(4);

    // 광 = 9, 10끗 = 6, 띠 = 5 기준.
    // 모든 섬 그룹의 총합이 30점이 되도록 고정 구성한다.
    Groups[0].Add(ECardID::Jan_Gwang);
    Groups[0].Add(ECardID::Aug_Yul);
    Groups[0].Add(ECardID::Feb_HongDdi);
    Groups[0].Add(ECardID::May_ChoDdi);
    Groups[0].Add(ECardID::Jul_ChoDdi);

    Groups[1].Add(ECardID::Mar_Gwang);
    Groups[1].Add(ECardID::May_Yul);
    Groups[1].Add(ECardID::Jan_HongDdi);
    Groups[1].Add(ECardID::Jun_CheongDdi);
    Groups[1].Add(ECardID::Oct_CheongDdi);

    Groups[2].Add(ECardID::Aug_Gwang);
    Groups[2].Add(ECardID::Feb_Yul);
    Groups[2].Add(ECardID::Mar_HongDdi);
    Groups[2].Add(ECardID::Apr_ChoDdi);
    Groups[2].Add(ECardID::Sep_CheongDdi);

    // 광이 없는 그룹은 10끗 5장으로 가치 보정한다.
    Groups[3].Add(ECardID::Apr_Yul);
    Groups[3].Add(ECardID::Jun_Yul);
    Groups[3].Add(ECardID::Jul_Yul);
    Groups[3].Add(ECardID::Sep_Yul);
    Groups[3].Add(ECardID::Oct_Yul);

    // 라운드마다 어떤 섬이 어떤 가치 그룹을 받는지 섞는다.
    for (int32 Index = Groups.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            Groups.Swap(Index, SwapIndex);
        }
    }

    // 같은 섬 안의 카드 위치 순서도 섞는다.
    for (TArray<ECardID>& Group : Groups)
    {
        ShuffleCardIDs(Group);
    }

    return Groups;
}

bool AMainGameMode::IsSeasonIslandActorName(const FString& ActorName) const
{
    return ActorName.Contains(TEXT("BPP_MAP_Summer"))
        || ActorName.Contains(TEXT("BPP_MAP_SPRING"))
        || ActorName.Contains(TEXT("BPP_MAP_Spring"))
        || ActorName.Contains(TEXT("BPP_MAP_Autumn"))
        || ActorName.Contains(TEXT("BPP_MAP_Winter"));
}


FVector AMainGameMode::GetRandomFallbackCardDropLocation(const TArray<FVector>& ExistingLocations) const
{
    const float SafeExtentX = FMath::Max(1.0f, CardBundleDropExtent.X);
    const float SafeExtentY = FMath::Max(1.0f, CardBundleDropExtent.Y);

    const int32 MaxAttempts = FMath::Max(1, CardIslandDropMaxAttemptsPerCard);
    const float MinDistance = FMath::Max(80.0f, CardIslandMinCardDistance);

    auto MakeCandidate = [this, SafeExtentX, SafeExtentY]() -> FVector
    {
        const float X = CardBundleDropCenter.X + FMath::FRandRange(-SafeExtentX, SafeExtentX);
        const float Y = CardBundleDropCenter.Y + FMath::FRandRange(-SafeExtentY, SafeExtentY);
        return FVector(X, Y, CardBundleDropCenter.Z);
    };

    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        const FVector Candidate = MakeCandidate();

        bool bTooClose = false;
        for (const FVector& Existing : ExistingLocations)
        {
            if (FVector::Dist2D(Candidate, Existing) < MinDistance)
            {
                bTooClose = true;
                break;
            }
        }

        if (!bTooClose)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Card RandomFallback Location=%s Existing=%d"),
                *Candidate.ToString(),
                ExistingLocations.Num());

            return Candidate;
        }
    }

    const FVector Fallback = MakeCandidate();

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card RandomFallback ForceLocation=%s Existing=%d"),
        *Fallback.ToString(),
        ExistingLocations.Num());

    return Fallback;
}
TArray<AMainGameMode::FCardIslandDropZone> AMainGameMode::FindCardIslandDropZones() const
{
    TArray<FCardIslandDropZone> TaggedZones;
    TArray<FCardIslandDropZone> AutoDetectedZones;

    UWorld* World = GetWorld();
    if (!World)
    {
        return TaggedZones;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        const bool bHasDropZoneTag = Actor->ActorHasTag(CardIslandDropZoneTag);
        const bool bIsAutoIslandActor = bAutoDetectSeasonIslandActorsAsDropZones && IsSeasonIslandActorName(Actor->GetName());

        if (!bHasDropZoneTag && !bIsAutoIslandActor)
        {
            continue;
        }

        const FBox Bounds = Actor->GetComponentsBoundingBox(true);
        if (!Bounds.IsValid)
        {
            continue;
        }

        const FVector Extent = Bounds.GetExtent();
        if (Extent.SizeSquared2D() < FMath::Square(100.0f))
        {
            continue;
        }

        FCardIslandDropZone Zone;
        Zone.ZoneActor = Actor;
        Zone.Bounds = Bounds;
        Zone.Center = Bounds.GetCenter();

        if (bHasDropZoneTag)
        {
            TaggedZones.Add(Zone);
        }
        else
        {
            AutoDetectedZones.Add(Zone);
        }
    }

    auto SortZones = [](TArray<FCardIslandDropZone>& Zones)
    {
        Zones.Sort([](const FCardIslandDropZone& A, const FCardIslandDropZone& B)
        {
            if (!FMath::IsNearlyEqual(A.Center.Y, B.Center.Y, 100.0f))
            {
                return A.Center.Y < B.Center.Y;
            }

            return A.Center.X < B.Center.X;
        });
    };

    SortZones(TaggedZones);
    SortZones(AutoDetectedZones);

    if (TaggedZones.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card DropZones FoundByTag Tag=%s Count=%d"),
            *CardIslandDropZoneTag.ToString(),
            TaggedZones.Num());

        return TaggedZones;
    }

    if (AutoDetectedZones.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card DropZones FoundByAutoDetect Count=%d"),
            AutoDetectedZones.Num());

        return AutoDetectedZones;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card DropZones NotFound Tag=%s AutoDetect=%d"),
        *CardIslandDropZoneTag.ToString(),
        bAutoDetectSeasonIslandActorsAsDropZones ? 1 : 0);

    return TaggedZones;
}

bool AMainGameMode::IsCardIslandSurfaceWalkable(const FHitResult& Hit) const
{
    if (!Hit.bBlockingHit)
    {
        return false;
    }

    const float ClampedSlopeDegrees = FMath::Clamp(CardIslandMaxGroundSlopeDegrees, 0.0f, 89.0f);
    const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(ClampedSlopeDegrees));

    return Hit.ImpactNormal.Z >= MinNormalZ;
}

bool AMainGameMode::IsCardDropLocationClear(const FVector& CandidateLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector SafeExtent(
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.X),
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.Y),
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.Z));

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;

    // CandidateLocation은 이미 바닥에서 CardIslandGroundOffsetZ만큼 띄운 스폰 위치다.
    // 체크 박스의 바닥이 스폰 위치보다 살짝 위에 오도록 해서 바닥 자체와 겹치는 오탐을 줄인다.
    const FVector CheckCenter = CandidateLocation + FVector(0.0f, 0.0f, SafeExtent.Z + 2.0f);
    const FCollisionShape CheckShape = FCollisionShape::MakeBox(SafeExtent);

    const bool bOverlapsBlockingObject = World->OverlapAnyTestByObjectType(
        CheckCenter,
        FQuat::Identity,
        ObjectQueryParams,
        CheckShape,
        QueryParams);

    return !bOverlapsBlockingObject;
}

bool AMainGameMode::IsFarEnoughFromIslandCards(const FVector& CandidateLocation, const TArray<FVector>& ExistingIslandLocations) const
{
    if (CardIslandMinCardDistance <= 0.0f)
    {
        return true;
    }

    const float MinDistanceSq = FMath::Square(CardIslandMinCardDistance);

    for (const FVector& ExistingLocation : ExistingIslandLocations)
    {
        if (FVector::DistSquared2D(CandidateLocation, ExistingLocation) < MinDistanceSq)
        {
            return false;
        }
    }

    return true;
}

bool AMainGameMode::TryResolveIslandCardDropLocation(
    const FCardIslandDropZone& DropZone,
    const FVector2D& CandidateXY,
    const TArray<FVector>& ExistingIslandLocations,
    FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector Start(CandidateXY.X, CandidateXY.Y, DropZone.Bounds.Max.Z + CardIslandGroundTraceHalfHeight);
    const FVector End(CandidateXY.X, CandidateXY.Y, DropZone.Bounds.Min.Z - CardIslandGroundTraceHalfHeight);

    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;

    if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams))
    {
        return false;
    }

    if (!IsCardIslandSurfaceWalkable(Hit))
    {
        return false;
    }

    FVector CandidateLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, CardIslandGroundOffsetZ);

    if (bProjectCardDropsToNavigation)
    {
        UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
        if (!NavSystem)
        {
            return false;
        }

        FNavLocation NavLocation;
        if (!NavSystem->ProjectPointToNavigation(CandidateLocation, NavLocation, CardIslandNavProjectExtent))
        {
            return false;
        }

        const FVector NavStart = NavLocation.Location + FVector(0.0f, 0.0f, CardIslandGroundTraceHalfHeight);
        const FVector NavEnd = NavLocation.Location - FVector(0.0f, 0.0f, CardIslandGroundTraceHalfHeight);

        FHitResult NavHit;
        if (!World->LineTraceSingleByChannel(NavHit, NavStart, NavEnd, ECC_WorldStatic, QueryParams))
        {
            return false;
        }

        if (!IsCardIslandSurfaceWalkable(NavHit))
        {
            return false;
        }

        CandidateLocation = NavHit.ImpactPoint + FVector(0.0f, 0.0f, CardIslandGroundOffsetZ);
    }

    if (!IsCardDropLocationClear(CandidateLocation))
    {
        return false;
    }

    if (!IsFarEnoughFromIslandCards(CandidateLocation, ExistingIslandLocations))
    {
        return false;
    }

    OutLocation = CandidateLocation;
    return true;
}

FVector AMainGameMode::PickIslandCardDropLocation(const FCardIslandDropZone& DropZone, const TArray<FVector>& ExistingIslandLocations) const
{
    const int32 MaxAttempts = FMath::Max(1, CardIslandDropMaxAttemptsPerCard);

    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        const float X = FMath::FRandRange(DropZone.Bounds.Min.X, DropZone.Bounds.Max.X);
        const float Y = FMath::FRandRange(DropZone.Bounds.Min.Y, DropZone.Bounds.Max.Y);

        FVector ResolvedLocation;
        if (TryResolveIslandCardDropLocation(DropZone, FVector2D(X, Y), ExistingIslandLocations, ResolvedLocation))
        {
            return ResolvedLocation;
        }
    }

    // 실패 시에도 게임 진행이 막히지 않도록 Zone 중심 근처로 fallback 한다.
    FVector FallbackLocation;
    if (TryResolveIslandCardDropLocation(DropZone, FVector2D(DropZone.Center.X, DropZone.Center.Y), ExistingIslandLocations, FallbackLocation))
    {
        return FallbackLocation;
    }

    return DropZone.Center + FVector(0.0f, 0.0f, CardIslandGroundOffsetZ);
}

int32 AMainGameMode::CreateCardInstance(ECardID CardID)
{
    if (CardID == ECardID::None)
    {
        return 0;
    }

    const int32 NewInstanceId = NextCardInstanceId++;

    FServerCardRecord Record;
    Record.CardInstanceId = NewInstanceId;
    Record.CardID = CardID;
    Record.State = ECardRuntimeState::Removed;
    Record.CreatedRound = CurrentRound;

    ServerCardRecords.Add(NewInstanceId, Record);

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card Create Instance=%d Card=%d Name=%s Round=%d"),
        NewInstanceId,
        static_cast<int32>(CardID),
        *CardDebug::ToString(CardID),
        CurrentRound);

    return NewInstanceId;
}

ACardDropActor* AMainGameMode::SpawnCardDrop(ECardID CardID, const FVector& SpawnLocation)
{
    if (!HasAuthority() || !GetWorld() || CardID == ECardID::None)
    {
        return nullptr;
    }

    TSubclassOf<ACardDropActor> SpawnClass = CardDropActorClass;
    if (!SpawnClass)
    {
        SpawnClass = ACardDropActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACardDropActor* CardActor = GetWorld()->SpawnActor<ACardDropActor>(SpawnClass, SpawnLocation, FRotator::ZeroRotator, Params);
    if (!CardActor)
    {
        return nullptr;
    }

    const int32 InstanceId = CreateCardInstance(CardID);
    if (InstanceId <= 0)
    {
        CardActor->Destroy();
        return nullptr;
    }

    CardActor->InitCardDrop(InstanceId, CardID);
    ActiveCardDrops.Add(CardActor);

    FServerCardRecord* Record = ServerCardRecords.Find(InstanceId);
    if (Record)
    {
        Record->State = ECardRuntimeState::WorldDrop;
        Record->DropActor = CardActor;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card Drop Instance=%d Card=%d Name=%s Actor=%s Location=%s Round=%d"),
        InstanceId,
        static_cast<int32>(CardID),
        *CardDebug::ToString(CardID),
        *CardActor->GetName(),
        *SpawnLocation.ToString(),
        CurrentRound);

    return CardActor;
}

void AMainGameMode::SpawnRoundCardBundleForBattleRoyale()
{
    if (!HasAuthority())
    {
        return;
    }

    ClearCardDrops();

    TArray<FCardIslandDropZone> IslandDropZones = FindCardIslandDropZones();
    TArray<TArray<ECardID>> IslandCardGroups = BuildBalancedIslandCardGroups();

    if (IslandDropZones.Num() < IslandCardGroups.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandDropFallback Reason=NotEnoughDropZones Found=%d Required=%d"),
            IslandDropZones.Num(),
            IslandCardGroups.Num());

        TArray<ECardID> CardIDs = BuildCardBundleIDs();
        ShuffleCardIDs(CardIDs);

        TArray<FVector> ExistingFallbackLocations;
        ExistingFallbackLocations.Reserve(CardIDs.Num());

        for (int32 Index = 0; Index < CardIDs.Num(); ++Index)
        {
            const FVector SpawnLocation = GetRandomFallbackCardDropLocation(ExistingFallbackLocations);
            ExistingFallbackLocations.Add(SpawnLocation);

            SpawnCardDrop(CardIDs[Index], SpawnLocation);
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card BundleDropComplete Count=%d Round=%d Phase=%s"),
            ActiveCardDrops.Num(),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    if (IslandDropZones.Num() != CardIslandDropExpectedZoneCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandDropZoneCountWarning Found=%d Expected=%d"),
            IslandDropZones.Num(),
            CardIslandDropExpectedZoneCount);
    }

    if (IslandDropZones.Num() > IslandCardGroups.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandDropExtraZonesIgnored Found=%d Used=%d"),
            IslandDropZones.Num(),
            IslandCardGroups.Num());
    }

    int32 PlannedCount = 0;

    for (int32 IslandIndex = 0; IslandIndex < IslandCardGroups.Num(); ++IslandIndex)
    {
        const FCardIslandDropZone& DropZone = IslandDropZones[IslandIndex];
        const TArray<ECardID>& CardsInIsland = IslandCardGroups[IslandIndex];
        const int32 BalanceValue = GetCardIslandGroupBalanceValue(CardsInIsland);

        AActor* ZoneActor = DropZone.ZoneActor.Get();
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandGroup Island=%d Zone=%s Cards=%d BalanceValue=%d BoundsCenter=%s BoundsExtent=%s"),
            IslandIndex,
            ZoneActor ? *ZoneActor->GetName() : TEXT("None"),
            CardsInIsland.Num(),
            BalanceValue,
            *DropZone.Center.ToString(),
            *DropZone.Bounds.GetExtent().ToString());

        TArray<FVector> ExistingIslandLocations;

        for (int32 SlotIndex = 0; SlotIndex < CardsInIsland.Num(); ++SlotIndex)
        {
            const ECardID CardID = CardsInIsland[SlotIndex];
            const FVector SpawnLocation = PickIslandCardDropLocation(DropZone, ExistingIslandLocations);

            ExistingIslandLocations.Add(SpawnLocation);
            SpawnCardDrop(CardID, SpawnLocation);
            ++PlannedCount;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandDropComplete Spawned=%d Planned=%d Islands=%d Round=%d Phase=%s"),
        ActiveCardDrops.Num(),
        PlannedCount,
        IslandCardGroups.Num(),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::ClearCardDrops()
{
    int32 ClearCount = 0;

    for (TObjectPtr<ACardDropActor> CardActorPtr : ActiveCardDrops)
    {
        ACardDropActor* CardActor = CardActorPtr.Get();
        if (!IsValid(CardActor))
        {
            continue;
        }

        if (FServerCardRecord* Record = ServerCardRecords.Find(CardActor->GetCardInstanceId()))
        {
            if (Record->State == ECardRuntimeState::WorldDrop)
            {
                Record->State = ECardRuntimeState::Removed;
                Record->DropActor = nullptr;
            }
        }

        CardActor->Destroy();
        ClearCount++;
    }

    ActiveCardDrops.Empty();

    if (ClearCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card ClearDrops Count=%d Round=%d"), ClearCount, CurrentRound);
    }
}


void AMainGameMode::EnsureThreeCardsForCardGame()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<int32> SupplementRecordIds;
    for (const TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        const FServerCardRecord& Record = Pair.Value;
        if (Record.CreatedRound == CurrentRound && Record.State == ECardRuntimeState::Removed && Record.CardID != ECardID::None)
        {
            SupplementRecordIds.Add(Pair.Key);
        }
    }

    for (int32 Index = SupplementRecordIds.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            SupplementRecordIds.Swap(Index, SwapIndex);
        }
    }

    int32 TargetCount = 0;
    int32 SupplementIndex = 0;
    const int32 TargetCardCount = FMath::Max(0, MaxCardsPerPlayerPerRound);

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
        if (!PS)
        {
            continue;
        }

        while (PS->OwnedCards.Num() < TargetCardCount)
        {
            bool bGranted = false;

            while (SupplementIndex < SupplementRecordIds.Num())
            {
                const int32 InstanceId = SupplementRecordIds[SupplementIndex++];
                if (GrantCardRecordToPlayer(InstanceId, PS, TEXT("AutoFill")))
                {
                    bGranted = true;
                    break;
                }
            }

            if (!bGranted)
            {
                bGranted = GrantNewCardToPlayer(PS, PickSupplementCardIDForPlayer(PS), TEXT("AutoFillFallback"));
            }

            if (!bGranted)
            {
                break;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card EnsureThree Player=%s Count=%d Max=%d Round=%d Cards=[%s]"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            TargetCardCount,
            CurrentRound,
            *GetOwnedCardsDebugString(PS));

        TargetCount++;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card EnsureThreeComplete targets=%d round=%d"), TargetCount, CurrentRound);
}

void AMainGameMode::ClearRoundCardsForAllPlayers()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    int32 TargetCount = 0;
    int32 CardCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
        if (!PS)
        {
            continue;
        }

        for (const FOwnedCardInfo& CardInfo : PS->OwnedCards)
        {
            if (FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId))
            {
                Record->State = ECardRuntimeState::Removed;
                Record->OwnerPlayerState = nullptr;
                Record->DropActor = nullptr;
            }
        }

        CardCount += PS->OwnedCards.Num();
        PS->ClearOwnedCards();
        TargetCount++;
    }

    SeotdaRoundStates.Empty();

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card ClearRoundCards targets=%d cards=%d round=%d"), TargetCount, CardCount, CurrentRound);
}

bool AMainGameMode::GrantCardRecordToPlayer(int32 CardInstanceId, AMainPlayerState* TargetPS, const TCHAR* Context)
{
    if (!HasAuthority() || !TargetPS || CardInstanceId <= 0)
    {
        return false;
    }

    if (TargetPS->OwnedCards.Num() >= MaxCardsPerPlayerPerRound)
    {
        return false;
    }

    FServerCardRecord* Record = ServerCardRecords.Find(CardInstanceId);
    if (!Record || Record->CardID == ECardID::None || Record->State == ECardRuntimeState::Owned || Record->State == ECardRuntimeState::Used)
    {
        return false;
    }

    FOwnedCardInfo CardInfo;
    CardInfo.CardInstanceId = Record->CardInstanceId;
    CardInfo.CardID = Record->CardID;

    TargetPS->AddOwnedCard(CardInfo);

    Record->State = ECardRuntimeState::Owned;
    Record->OwnerPlayerState = TargetPS;
    Record->DropActor = nullptr;

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card Grant Player=%s Instance=%d Card=%d Context=%s Count=%d"),
        *TargetPS->GetPlayerName(),
        CardInfo.CardInstanceId,
        static_cast<int32>(CardInfo.CardID),
        Context ? Context : TEXT("<NULL>"),
        TargetPS->PublicCardCount);

    return true;
}

bool AMainGameMode::GrantNewCardToPlayer(AMainPlayerState* TargetPS, ECardID CardID, const TCHAR* Context)
{
    if (!HasAuthority() || !TargetPS || CardID == ECardID::None)
    {
        return false;
    }

    const int32 InstanceId = CreateCardInstance(CardID);
    if (InstanceId <= 0)
    {
        return false;
    }

    return GrantCardRecordToPlayer(InstanceId, TargetPS, Context);
}

ECardID AMainGameMode::PickSupplementCardIDForPlayer(const AMainPlayerState* TargetPS) const
{
    TArray<ECardID> CardIDs = BuildCardBundleIDs();
    if (CardIDs.Num() == 0)
    {
        return ECardID::None;
    }

    if (TargetPS)
    {
        CardIDs.RemoveAll([TargetPS](ECardID Candidate)
        {
            return TargetPS->OwnedCards.ContainsByPredicate([Candidate](const FOwnedCardInfo& CardInfo)
            {
                return CardInfo.CardID == Candidate;
            });
        });
    }

    if (CardIDs.Num() == 0)
    {
        CardIDs = BuildCardBundleIDs();
    }

    if (CardIDs.Num() == 0)
    {
        return ECardID::None;
    }

    return CardIDs[FMath::RandRange(0, CardIDs.Num() - 1)];
}

bool AMainGameMode::IsCardPickupAllowed() const
{
    return CurrentServerPhase == EDediServerPhase::BattleRoyale;
}

void AMainGameMode::SetPlayerPawnGameplayEnabled(bool bEnabled, const TCHAR* Context)
{
    int32 TargetCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        APawn* Pawn = PC->GetPawn();
        if (!Pawn)
        {
            continue;
        }

        if (ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                MoveComp->SetBase(nullptr);
                MoveComp->StopMovementImmediately();

                if (bEnabled)
                {
                    MoveComp->SetMovementMode(MOVE_Walking);
                }
                else
                {
                    MoveComp->DisableMovement();
                }
            }
        }

        Pawn->SetReplicateMovement(bEnabled);
        Pawn->SetActorEnableCollision(bEnabled);
        Pawn->SetActorHiddenInGame(!bEnabled);
        Pawn->ForceNetUpdate();
        TargetCount++;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main SetPlayerPawnGameplayEnabled enabled=%d targets=%d Context=%s Round=%d ServerPhase=%s"),
        bEnabled ? 1 : 0,
        TargetCount,
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::ClearPlayerPawnMovementBases(const TCHAR* Context)
{
    int32 TargetCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        APawn* Pawn = PC->GetPawn();
        if (!Pawn)
        {
            continue;
        }

        if (ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                MoveComp->SetBase(nullptr);
                MoveComp->StopMovementImmediately();
            }
        }

        Pawn->ForceNetUpdate();
        TargetCount++;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main ClearPlayerPawnMovementBases targets=%d Context=%s Round=%d ServerPhase=%s"),
        TargetCount,
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::StartTimedServerPhase(EDediServerPhase NewPhase, int32 DurationSeconds)
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTimedServerPhase phase=%s after GameEnd Round=%d"),
            GetServerPhaseName(NewPhase),
            CurrentRound);
        return;
    }

    ClearServerPhaseTimer();

    CurrentServerPhase = NewPhase;
    RemainingPhaseSeconds = FMath::Max(0, DurationSeconds);
    SetServerRemainingTime(RemainingPhaseSeconds);

    UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseStart Round=%d Phase=%s Duration=%d Debug=%d"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        RemainingPhaseSeconds,
        bUseDebugPhaseDurations ? 1 : 0);

    if (RemainingPhaseSeconds <= 0)
    {
        FinishCurrentServerPhase(TEXT("ZeroDuration"));
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PhaseTimerHandle,
            this,
            &AMainGameMode::OnServerPhaseTick,
            1.0f,
            true);
    }
}

void AMainGameMode::OnServerPhaseTick()
{
    if (bGameEndReached || CurrentServerPhase == EDediServerPhase::GameEnd)
    {
        ClearServerPhaseTimer();
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard ClearTick after GameEnd Round=%d"), CurrentRound);
        return;
    }

    RemainingPhaseSeconds = FMath::Max(0, RemainingPhaseSeconds - 1);
    SetServerRemainingTime(RemainingPhaseSeconds);

    if (RemainingPhaseSeconds <= 5 || RemainingPhaseSeconds % 10 == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseTick Round=%d Phase=%s Remaining=%d"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase),
            RemainingPhaseSeconds);
    }

    if (RemainingPhaseSeconds <= 0)
    {
        FinishCurrentServerPhase(TEXT("Timeout"));
    }
}

void AMainGameMode::FinishCurrentServerPhase(const TCHAR* Reason)
{
    if (bGameEndReached || CurrentServerPhase == EDediServerPhase::GameEnd)
    {
        ClearServerPhaseTimer();
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore FinishCurrentServerPhase after GameEnd Round=%d Reason=%s"),
            CurrentRound,
            Reason ? Reason : TEXT("<NULL>"));
        return;
    }

    const EDediServerPhase FinishedPhase = CurrentServerPhase;

    ClearServerPhaseTimer();

    UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseEnd Round=%d Phase=%s Reason=%s"),
        CurrentRound,
        GetServerPhaseName(FinishedPhase),
        Reason);

    switch (FinishedPhase)
    {
    case EDediServerPhase::Ready:
        StartBattleRoyalePhase();
        break;
    case EDediServerPhase::BattleRoyale:
        StartTransitionToCardPhase();
        break;
    case EDediServerPhase::TransitionToCard:
        StartCardGamePhase();
        break;
    case EDediServerPhase::CardGame:
        StartResultPhase();
        break;
    case EDediServerPhase::Result:
        if (CurrentRound >= MaxRoundCount)
        {
            StartGameEndPhase();
        }
        else
        {
            CurrentRound++;
            UE_LOG(LogTemp, Warning, TEXT("[DS] NextRound Round=%d/%d"), CurrentRound, MaxRoundCount);
            StartTransitionToBattlePhase();
        }
        break;
    case EDediServerPhase::TransitionToBattle:
        StartBattleRoyalePhase();
        break;
    default:
        break;
    }
}

void AMainGameMode::ClearServerPhaseTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTimerHandle);
    }
}

void AMainGameMode::SetServerRemainingTime(int32 NewTime)
{
    if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld() ? GetWorld()->GetGameState() : nullptr))
    {
        GS->SetRemainingTime(NewTime);
        GS->BroadcastTimeUpdated(NewTime);
    }
}

int32 AMainGameMode::GetReadyDuration() const
{
    return bUseDebugPhaseDurations ? DebugReadyDuration : RealReadyDuration;
}

int32 AMainGameMode::GetBattleRoyaleDuration() const
{
    return bUseDebugPhaseDurations ? DebugBattleRoyaleDuration : RealBattleRoyaleDuration;
}

int32 AMainGameMode::GetTransitionDuration() const
{
    return bUseDebugPhaseDurations ? DebugTransitionDuration : RealTransitionDuration;
}

int32 AMainGameMode::GetCardGameDuration() const
{
    return bUseDebugPhaseDurations ? DebugCardGameDuration : RealCardGameDuration;
}

int32 AMainGameMode::GetResultDuration() const
{
    return bUseDebugPhaseDurations ? DebugResultDuration : RealResultDuration;
}

const TCHAR* AMainGameMode::GetServerPhaseName(EDediServerPhase Phase) const
{
    switch (Phase)
    {
    case EDediServerPhase::Ready:
        return TEXT("Ready");
    case EDediServerPhase::BattleRoyale:
        return TEXT("BattleRoyale");
    case EDediServerPhase::TransitionToCard:
        return TEXT("TransitionToCard");
    case EDediServerPhase::CardGame:
        return TEXT("CardGame");
    case EDediServerPhase::Result:
        return TEXT("Result");
    case EDediServerPhase::TransitionToBattle:
        return TEXT("TransitionToBattle");
    case EDediServerPhase::GameEnd:
        return TEXT("GameEnd");
    default:
        return TEXT("None");
    }
}

namespace
{
    static void ManagerAppendU8(TArray<uint8>& Out, uint8 Value)
    {
        Out.Add(Value);
    }

    static void ManagerAppendU16BE(TArray<uint8>& Out, uint16 Value)
    {
        Out.Add(static_cast<uint8>((Value >> 8) & 0xFF));
        Out.Add(static_cast<uint8>(Value & 0xFF));
    }

    static void ManagerAppendU32BE(TArray<uint8>& Out, uint32 Value)
    {
        Out.Add(static_cast<uint8>((Value >> 24) & 0xFF));
        Out.Add(static_cast<uint8>((Value >> 16) & 0xFF));
        Out.Add(static_cast<uint8>((Value >> 8) & 0xFF));
        Out.Add(static_cast<uint8>(Value & 0xFF));
    }

    static void ManagerAppendUtf8Limited(TArray<uint8>& Out, const FString& Text, int32 MaxLen)
    {
        FTCHARToUTF8 Converted(*Text);

        const int32 RawLen = Converted.Length();
        const int32 UseLen = FMath::Clamp(RawLen, 0, MaxLen);

        Out.Add(static_cast<uint8>(UseLen));

        if (UseLen > 0)
        {
            const uint8* Data = reinterpret_cast<const uint8*>(Converted.Get());
            Out.Append(Data, UseLen);
        }
    }
}

void AMainGameMode::NotifyIocpMatchEnd(const FString& WinnerName, const FString& MoneySummary) const
{
    constexpr uint16 MatchEndPacketType = 300;
    constexpr TCHAR IocpHost[] = TEXT("127.0.0.1");
    constexpr int32 IocpPort = 9000;

    if (DediRoomId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] IOCP MatchEndNotify skipped. Invalid RoomId=%d Winner=%s"),
            DediRoomId,
            *WinnerName);
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendUtf8Limited(Payload, WinnerName, 96);
    ManagerAppendUtf8Limited(Payload, MoneySummary, 180);

    TArray<uint8> Packet;
    const uint16 TotalSize = static_cast<uint16>(4 + Payload.Num());

    ManagerAppendU16BE(Packet, TotalSize);
    ManagerAppendU16BE(Packet, MatchEndPacketType);
    Packet.Append(Payload);

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchEndNotify failed. No SocketSubsystem RoomId=%d"), DediRoomId);
        return;
    }

    TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();

    bool bIpValid = false;
    Addr->SetIp(IocpHost, bIpValid);
    Addr->SetPort(IocpPort);

    if (!bIpValid)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchEndNotify failed. Invalid IP=%s"), IocpHost);
        return;
    }

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("ManagerIocpMatchEndNotify"), false);
    if (!Socket)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchEndNotify failed. CreateSocket RoomId=%d"), DediRoomId);
        return;
    }

    Socket->SetNonBlocking(false);

    bool bConnected = Socket->Connect(*Addr);
    int32 BytesSent = 0;
    bool bSent = false;

    if (bConnected)
    {
        bSent = Socket->Send(Packet.GetData(), Packet.Num(), BytesSent);
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] IOCP MatchEndNotify RoomId=%d Winner=%s Money=%s Connected=%d Sent=%d Bytes=%d/%d"),
        DediRoomId,
        *WinnerName,
        *MoneySummary,
        bConnected ? 1 : 0,
        bSent ? 1 : 0,
        BytesSent,
        Packet.Num());

    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);
}

int32 AMainGameMode::CompareSeotdaHands(const FSeotdaHandResult& A, const FSeotdaHandResult& B) const
{
    auto IsSamPalGwangDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank == 12000;
    };

    auto IsGwangDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank == 11000;
    };

    auto IsNormalDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank >= 10001 && H.Rank <= 10010;
    };

    if (IsSamPalGwangDdang(A) || IsSamPalGwangDdang(B))
    {
        if (IsSamPalGwangDdang(A) && !IsSamPalGwangDdang(B)) return 1;
        if (!IsSamPalGwangDdang(A) && IsSamPalGwangDdang(B)) return -1;
    }

    if (A.SpecialRule == ESeotdaSpecialRule::AmhaengEosa && IsGwangDdang(B))
    {
        return 1;
    }

    if (B.SpecialRule == ESeotdaSpecialRule::AmhaengEosa && IsGwangDdang(A))
    {
        return -1;
    }

    if (A.SpecialRule == ESeotdaSpecialRule::TtaengJabi && IsNormalDdang(B))
    {
        return 1;
    }

    if (B.SpecialRule == ESeotdaSpecialRule::TtaengJabi && IsNormalDdang(A))
    {
        return -1;
    }

    if (A.Rank != B.Rank)
    {
        return A.Rank > B.Rank ? 1 : -1;
    }

    if (A.SubRank != B.SubRank)
    {
        return A.SubRank > B.SubRank ? 1 : -1;
    }

    return 0;
}

bool AMainGameMode::ShouldForceSeotdaRedeal() const
{
    for (const TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        const FSeotdaPlayerRoundState& State = Pair.Value;

        if (!State.bSubmitted || State.bFolded)
        {
            continue;
        }

        if (State.HandResult.bForcesRedeal)
        {
            return true;
        }
    }

    return false;
}

bool AMainGameMode::TryApplySeotdaRedealFromRemainingCards(const TCHAR* Reason)
{
    if (!HasAuthority() || !GetWorld())
    {
        return false;
    }

    TArray<AMainPlayerState*> ActivePlayers;

    for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        AMainPlayerState* PS = Pair.Key;
        FSeotdaPlayerRoundState& State = Pair.Value;

        if (!PS || !State.bSubmitted || State.bFolded)
        {
            continue;
        }

        ActivePlayers.Add(PS);
    }

    const int32 NeedCardCount = ActivePlayers.Num() * 2;
    if (NeedCardCount <= 0)
    {
        return false;
    }

    TArray<int32> RemainingRecordIds;

    for (const TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        const FServerCardRecord& Record = Pair.Value;

        if (Record.CreatedRound == CurrentRound &&
            Record.State == ECardRuntimeState::WorldDrop &&
            Record.CardID != ECardID::None)
        {
            RemainingRecordIds.Add(Pair.Key);
        }
    }

    for (int32 Index = RemainingRecordIds.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        RemainingRecordIds.Swap(Index, SwapIndex);
    }

    if (RemainingRecordIds.Num() < NeedCardCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealReject Reason=NotEnoughCards Need=%d Remain=%d Round=%d"),
            NeedCardCount,
            RemainingRecordIds.Num(),
            CurrentRound);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealStart Reason=%s ActivePlayers=%d NeedCards=%d RemainCards=%d Round=%d Pot=%d"),
        Reason ? Reason : TEXT("<NULL>"),
        ActivePlayers.Num(),
        NeedCardCount,
        RemainingRecordIds.Num(),
        CurrentRound,
        SeotdaPot);

    int32 DrawIndex = 0;

    for (AMainPlayerState* PS : ActivePlayers)
    {
        if (!PS)
        {
            continue;
        }

        const int32 FirstRecordId = RemainingRecordIds[DrawIndex++];
        const int32 SecondRecordId = RemainingRecordIds[DrawIndex++];

        FServerCardRecord* FirstRecord = ServerCardRecords.Find(FirstRecordId);
        FServerCardRecord* SecondRecord = ServerCardRecords.Find(SecondRecordId);

        if (!FirstRecord || !SecondRecord)
        {
            continue;
        }

        FOwnedCardInfo FirstInfo;
        FirstInfo.CardInstanceId = FirstRecordId;
        FirstInfo.CardID = FirstRecord->CardID;

        FOwnedCardInfo SecondInfo;
        SecondInfo.CardInstanceId = SecondRecordId;
        SecondInfo.CardID = SecondRecord->CardID;

        FirstRecord->State = ECardRuntimeState::Used;
        FirstRecord->OwnerPlayerState = PS;

        SecondRecord->State = ECardRuntimeState::Used;
        SecondRecord->OwnerPlayerState = PS;

        if (FirstRecord->DropActor.IsValid())
        {
            FirstRecord->DropActor->Destroy();
            FirstRecord->DropActor.Reset();
        }

        if (SecondRecord->DropActor.IsValid())
        {
            SecondRecord->DropActor->Destroy();
            SecondRecord->DropActor.Reset();
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State)
        {
            continue;
        }

        State->SelectedCardInstanceIds.Empty();
        State->SelectedCardInstanceIds.Add(FirstRecordId);
        State->SelectedCardInstanceIds.Add(SecondRecordId);
        State->HandResult = EvaluateSeotdaHand(FirstInfo, SecondInfo);
        State->bSubmitted = true;
        State->bActedThisBetRound = true;

        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealCard Player=%s Cards=%d:%s,%d:%s Combo=%s Rank=%d SubRank=%d"),
            *PS->GetPlayerName(),
            FirstRecordId,
            *CardDebug::ToString(FirstInfo.CardID),
            SecondRecordId,
            *CardDebug::ToString(SecondInfo.CardID),
            *State->HandResult.Name,
            State->HandResult.Rank,
            State->HandResult.SubRank);
    }

    return true;
}

void AMainGameMode::ShutdownDedicatedServerAfterMatchEnd()
{
    UE_LOG(LogTemp, Warning, TEXT("[DS] ShutdownDedicatedServerAfterMatchEnd RoomId=%d Round=%d Phase=%s"),
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    FPlatformMisc::RequestExit(false);
}

AActor* AMainGameMode::ChoosePlayerStart_Implementation(AController* Player)
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
APawn* AMainGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
    FTransform OffsetTransform = SpawnTransform;
    FVector NewLocation = OffsetTransform.GetLocation() + FVector(0.0f, 0.0f, 100.0f);
    OffsetTransform.SetLocation(NewLocation);
    return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OffsetTransform);
}
