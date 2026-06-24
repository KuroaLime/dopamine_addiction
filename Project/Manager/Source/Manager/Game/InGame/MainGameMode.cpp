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
    AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    if (GS)
    {
        GS->CurrentRound = CurrentRound;
        GS->OnRep_CurrentRound();
    }
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
    BroadcastSwitchLevel(TEXT("Test"), TEXT("Card_Game_Stage"));
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
    BroadcastSwitchLevel(TEXT("Card_Game_Stage"), TEXT("Test"));
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

    // 湲곕낯 ?먮룉? ?쒕쾭媛 ?ｋ뒗?? ?뚮젅?댁뼱 ?덉뿉?쒕뒗 鍮좎?吏 ?딅뒗??
    SeotdaPot = FMath::Max(0, SeotdaServerSeedPot);

    // Call???뚮?????媛??뚮젅?댁뼱媛 湲곕낯 2?먯쓣 ?대룄濡??쒖옉 湲곗? 踰좏똿??2濡??붾떎.
    SeotdaCurrentBet = FMath::Max(0, SeotdaBaseCallBet);

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

    // 湲곕낯 ?먮룉? ?쒕쾭媛 ?ｋ뒗?? ?뚮젅?댁뼱 ?덉뿉?쒕뒗 鍮좎?吏 ?딅뒗??
    SeotdaPot = FMath::Max(0, SeotdaServerSeedPot);

    // Call???뚮?????媛??뚮젅?댁뼱媛 湲곕낯 2?먯쓣 ?대룄濡??쒖옉 湲곗? 踰좏똿??2濡??붾떎.
    SeotdaCurrentBet = FMath::Max(0, SeotdaBaseCallBet);

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
    case EBettingAction::Quarter:
        RequestedPay = CallAmount + FMath::Max(1, (SeotdaPot + CallAmount) / 4);
        break;
    case EBettingAction::Half:
        RequestedPay = CallAmount + FMath::Max(1, (SeotdaPot + CallAmount) / 2);
        break;
    case EBettingAction::Ddadang:
        RequestedPay = CallAmount + FMath::Max(SeotdaBaseCallBet, SeotdaCurrentBet);
        break;
    case EBettingAction::Pping:
        RequestedPay = (CallAmount > 0) ? CallAmount : SeotdaBaseCallBet;
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
    // 1?? 愿? ?띾씈
    CardIDs.Add(ECardID::Jan_Gwang);
    CardIDs.Add(ECardID::Jan_HongDdi);

    // 2?? 10?? ?띾씈
    CardIDs.Add(ECardID::Feb_Yul);
    CardIDs.Add(ECardID::Feb_HongDdi);

    // 3?? 愿? ?띾씈
    CardIDs.Add(ECardID::Mar_Gwang);
    CardIDs.Add(ECardID::Mar_HongDdi);

    // 4?? 10?? 珥덈씈
    CardIDs.Add(ECardID::Apr_Yul);
    CardIDs.Add(ECardID::Apr_ChoDdi);

    // 5?? 10?? 珥덈씈
    CardIDs.Add(ECardID::May_Yul);
    CardIDs.Add(ECardID::May_ChoDdi);

    // 6?? 10?? 泥?씈
    CardIDs.Add(ECardID::Jun_Yul);
    CardIDs.Add(ECardID::Jun_CheongDdi);

    // 7?? 10?? 珥덈씈
    CardIDs.Add(ECardID::Jul_Yul);
    CardIDs.Add(ECardID::Jul_ChoDdi);

    // 8?? 愿? 10??
    CardIDs.Add(ECardID::Aug_Gwang);
    CardIDs.Add(ECardID::Aug_Yul);

    // 9?? 10?? 泥?씈
    CardIDs.Add(ECardID::Sep_Yul);
    CardIDs.Add(ECardID::Sep_CheongDdi);

    // 10?? 10?? 泥?씈
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

    // 湲곗〈 諛⑹떇? 20?μ씪 ??6x4=24移몄씠 ?섏뼱 留덉?留?以꾩씠 移섏슦移????덉뿀??
    // ??諛⑹떇? 20??湲곗? 5x4??媛源앷쾶 留뚮뱾???꾩껜 ?곸뿭????怨좊Ⅴ寃?諛곗튂?쒕떎.
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

    // 留덉?留?以꾩씠 苑?李⑥? ?딆븘??以묒븰 ?뺣젹?섍쾶 蹂댁젙
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

    // 愿?= 9, 10??= 6, ??= 5 湲곗?.
    // 紐⑤뱺 ??洹몃９??珥앺빀??30?먯씠 ?섎룄濡?怨좎젙 援ъ꽦?쒕떎.
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

    // 愿묒씠 ?녿뒗 洹몃９? 10??5?μ쑝濡?媛移?蹂댁젙?쒕떎.
    Groups[3].Add(ECardID::Apr_Yul);
    Groups[3].Add(ECardID::Jun_Yul);
    Groups[3].Add(ECardID::Jul_Yul);
    Groups[3].Add(ECardID::Sep_Yul);
    Groups[3].Add(ECardID::Oct_Yul);

    // ?쇱슫?쒕쭏???대뼡 ?ъ씠 ?대뼡 媛移?洹몃９??諛쏅뒗吏 ?욌뒗??
    for (int32 Index = Groups.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            Groups.Swap(Index, SwapIndex);
        }
    }

    // 媛숈? ???덉쓽 移대뱶 ?꾩튂 ?쒖꽌???욌뒗??
    for (TArray<ECardID>& Group : Groups)
    {
        ShuffleCardIDs(Group);
    }

    return Groups;
}

bool AMainGameMode::IsSeasonIslandActorName(const FString& ActorName) const
{
    return ActorName.Contains(TEXT("BPP_MAP_Summer"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Spring"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Autumn"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Winter"), ESearchCase::IgnoreCase);
}


TArray<AMainGameMode::FCardIslandDropZone> AMainGameMode::FindCardIslandDropZones() const
{
    TArray<FCardIslandDropZone> DropZones;

    UWorld* World = GetWorld();
    if (!World)
    {
        return DropZones;
    }

    auto GetActorSearchText = [](const AActor* Actor) -> FString
    {
        if (!IsValid(Actor))
        {
            return FString();
        }

        const FString ActorName = GetNameSafe(Actor);
        FString LabelName;
#if WITH_EDITOR
        LabelName = Actor->GetActorLabel();
#endif
        const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
        return ActorName + TEXT(" ") + LabelName + TEXT(" ") + ClassName;
    };

    auto GetSeasonIslandKey = [](const FString& SearchText, int32& OutSortOrder) -> FName
    {
        if (SearchText.Contains(TEXT("BPP_MAP_Winter"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 0;
            return FName(TEXT("Winter"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Spring"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 1;
            return FName(TEXT("Spring"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Summer"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 2;
            return FName(TEXT("Summer"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Autumn"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 3;
            return FName(TEXT("Autumn"));
        }

        OutSortOrder = 1000;
        return NAME_None;
    };

    auto MakeZone = [](AActor* Actor, const FVector& Origin, const FVector& Extent, FName IslandKey, const FString& Source, int32 SortOrder) -> FCardIslandDropZone
    {
        FCardIslandDropZone Zone;
        Zone.ZoneActor = Actor;
        Zone.Bounds = FBox(Origin - Extent, Origin + Extent);
        Zone.Center = Origin;
        Zone.IslandKey = IslandKey;
        Zone.Source = Source;
        Zone.SortOrder = SortOrder;
        return Zone;
    };

    TArray<FCardIslandDropZone> TaggedNavAreaZones;
    TMap<FName, FCardIslandDropZone> SeasonZonesByKey;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        FVector Origin;
        FVector Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        if (Extent.X < 500.0f || Extent.Y < 500.0f)
        {
            continue;
        }

        const FString SearchText = GetActorSearchText(Actor);
        const bool bTaggedNavArea = Actor->ActorHasTag(FName(TEXT("CardIslandNavArea")));

        int32 SeasonSortOrder = 1000;
        const FName SeasonKey = GetSeasonIslandKey(SearchText, SeasonSortOrder);

        if (bTaggedNavArea)
        {
            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, Actor->GetFName(), TEXT("CardIslandNavArea"), TaggedNavAreaZones.Num());
            TaggedNavAreaZones.Add(Zone);

            UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=CardIslandNavArea Actor=%s Key=%s Center=%s Extent=%s"),
                *GetNameSafe(Actor), *Zone.IslandKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString());
        }

        if (bAutoDetectSeasonIslandActorsAsDropZones && !SeasonKey.IsNone())
        {
            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, SeasonKey, TEXT("SeasonIslandActor"), SeasonSortOrder);
            FCardIslandDropZone* ExistingZone = SeasonZonesByKey.Find(SeasonKey);
            const float NewArea = Extent.X * Extent.Y;
            const float ExistingArea = ExistingZone ? ExistingZone->Bounds.GetExtent().X * ExistingZone->Bounds.GetExtent().Y : -1.0f;

            if (!ExistingZone || NewArea > ExistingArea)
            {
                SeasonZonesByKey.Add(SeasonKey, Zone);

                UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=SeasonIslandActor Actor=%s Key=%s Center=%s Extent=%s Selected=%d"),
                    *GetNameSafe(Actor), *SeasonKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString(), 1);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=SeasonIslandActor Actor=%s Key=%s Center=%s Extent=%s Selected=%d Reason=SmallerDuplicate"),
                    *GetNameSafe(Actor), *SeasonKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString(), 0);
            }
        }
    }

    if (TaggedNavAreaZones.Num() >= CardIslandDropExpectedZoneCount)
    {
        DropZones = TaggedNavAreaZones;
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=CardIslandNavArea Count=%d"), DropZones.Num());
    }
    else if (SeasonZonesByKey.Num() > 0)
    {
        SeasonZonesByKey.GenerateValueArray(DropZones);
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=SeasonIslandActor Count=%d TaggedNavAreas=%d"),
            DropZones.Num(), TaggedNavAreaZones.Num());
    }
    else if (TaggedNavAreaZones.Num() > 0)
    {
        DropZones = TaggedNavAreaZones;
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=PartialCardIslandNavArea Count=%d"), DropZones.Num());
    }

    // 혹시 섬 액터/NavArea를 못 찾으면 기존 TriggerBox 방식으로만 fallback
    if (DropZones.Num() == 0)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!IsValid(Actor))
            {
                continue;
            }

            if (!Actor->ActorHasTag(CardIslandDropZoneTag))
            {
                continue;
            }

            FVector Origin;
            FVector Extent;
            Actor->GetActorBounds(false, Origin, Extent);

            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, Actor->GetFName(), TEXT("TriggerBoxFallback"), DropZones.Num());

            DropZones.Add(Zone);

            UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=TriggerBoxFallback Actor=%s Key=%s Center=%s Extent=%s"),
                *GetNameSafe(Actor), *Zone.IslandKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString());
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas FallbackToTriggerBoxes Count=%d"), DropZones.Num());
    }

    DropZones.Sort([](const FCardIslandDropZone& A, const FCardIslandDropZone& B)
    {
        if (A.SortOrder != B.SortOrder)
        {
            return A.SortOrder < B.SortOrder;
        }

        if (!FMath::IsNearlyEqual(A.Center.Y, B.Center.Y))
        {
            return A.Center.Y < B.Center.Y;
        }

        return A.Center.X < B.Center.X;
    });

    for (int32 Index = 0; Index < DropZones.Num(); ++Index)
    {
        const FCardIslandDropZone& Zone = DropZones[Index];
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Selected Index=%d Source=%s Key=%s Actor=%s Center=%s Extent=%s"),
            Index, *Zone.Source, *Zone.IslandKey.ToString(), *GetNameSafe(Zone.ZoneActor.Get()),
            *Zone.Center.ToCompactString(), *Zone.Bounds.GetExtent().ToCompactString());
    }

    return DropZones;
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

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardIslandDropClear), false);
    QueryParams.bTraceComplex = false;

    const FCollisionShape CheckShape = FCollisionShape::MakeBox(SafeExtent);

    const bool bOverlapsBlockingObject = World->OverlapBlockingTestByChannel(
        CandidateLocation,
        FQuat::Identity,
        ECC_WorldDynamic,
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

bool AMainGameMode::IsCardDropZSane(const FCardIslandDropZone& DropZone, float ReferenceNavZ, const FVector& Candidate) const
{
    const float CandidateNavZ = Candidate.Z - CardIslandGroundOffsetZ;
    const float MaxDelta = FMath::Max(120.0f, CardIslandMaxGroundZDelta);

    if (FMath::Abs(CandidateNavZ - ReferenceNavZ) > MaxDelta)
    {
        return false;
    }

    const float BoundsPadding = FMath::Max(120.0f, CardIslandGroundOffsetZ + 40.0f);
    if (Candidate.Z < DropZone.Bounds.Min.Z - BoundsPadding)
    {
        return false;
    }

    if (Candidate.Z > DropZone.Bounds.Max.Z + BoundsPadding)
    {
        return false;
    }

    return true;
}

bool AMainGameMode::IsInsideNoDropZone(const FVector& Candidate) const
{
    UWorld* World = GetWorld();
    if (!World || CardNoDropZoneTag.IsNone())
    {
        return false;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || !Actor->ActorHasTag(CardNoDropZoneTag))
        {
            continue;
        }

        FVector Origin;
        FVector Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        const FBox NoBox(Origin - Extent, Origin + Extent);
        if (NoBox.IsInsideXY(Candidate))
        {
            return true;
        }
    }

    return false;
}

bool AMainGameMode::HasOverheadClearance(const FVector& Candidate) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return true;
    }

    // 보조 검증용 LineTrace: 카드 바로 위로 짧게 쏘아 머리 위가 막혀 있으면(캐노피/바위 밑) 제외한다.
    // Spawn Z 자체는 NavMesh 지면 Z 기준이므로 이 트레이스는 Z를 바꾸지 않는다.
    const float ClearHeight = FMath::Max(1.0f, CardIslandOverheadClearance);
    const FVector Start = Candidate;
    const FVector End = Candidate + FVector(0.0f, 0.0f, ClearHeight);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardIslandDropOverhead), false);
    QueryParams.bTraceComplex = false;

    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldDynamic, QueryParams);
    return !bBlocked;
}

bool AMainGameMode::PickIslandCardDropLocation(const FCardIslandDropZone& DropZone, const TArray<FVector>& ExistingIslandLocations, int32 IslandIndex, int32 SlotIndex, FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Reason=NoWorld"), IslandIndex, SlotIndex);
        return false;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Reason=NoNavSystem"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()));
        return false;
    }

    const FVector Extent = DropZone.Bounds.GetExtent();
    const FVector Center = DropZone.Center;

    const FVector AnchorProjectExtent(
        FMath::Max(400.0f, CardIslandNavProjectExtent.X),
        FMath::Max(400.0f, CardIslandNavProjectExtent.Y),
        FMath::Max(1200.0f, FMath::Max(CardIslandNavProjectExtent.Z, Extent.Z + 400.0f)));

    const FVector CandidateProjectExtent(
        FMath::Max(200.0f, CardIslandNavProjectExtent.X),
        FMath::Max(200.0f, CardIslandNavProjectExtent.Y),
        FMath::Max(500.0f, CardIslandNavProjectExtent.Z));

    const float MinDistance = FMath::Max(1.0f, CardIslandMinCardDistance);
    const int32 MaxAttempts = FMath::Max(160, CardIslandDropMaxAttemptsPerCard);
    const float VisibleRadius = FMath::Clamp(FMath::Min(Extent.X, Extent.Y) * 0.36f, 650.0f, 950.0f);
    const float PatternRadius = FMath::Clamp(MinDistance * 2.4f, 560.0f, 700.0f);
    const float RandomRadius = FMath::Clamp(VisibleRadius * 0.55f, 420.0f, 650.0f);

    TArray<FVector> NavAnchors;
    int32 AnchorNavFail = 0;
    int32 AnchorBoundsFail = 0;

    auto TryAddNavAnchor = [&](const FVector& QueryPoint, const TCHAR* Source) -> void
    {
        FNavLocation NavLocation;
        if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, AnchorProjectExtent))
        {
            ++AnchorNavFail;
            return;
        }

        if (!DropZone.Bounds.IsInsideXY(NavLocation.Location))
        {
            ++AnchorBoundsFail;
            return;
        }

        for (const FVector& ExistingAnchor : NavAnchors)
        {
            if (FVector::DistSquared2D(ExistingAnchor, NavLocation.Location) < FMath::Square(100.0f))
            {
                return;
            }
        }

        NavAnchors.Add(NavLocation.Location);

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card NavIsland Anchor Island=%d Slot=%d Zone=%s Key=%s Source=%s Location=%s NavZ=%.1f"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), Source,
            *NavLocation.Location.ToCompactString(), NavLocation.Location.Z);
    };

    TryAddNavAnchor(Center, TEXT("Center"));

    const FVector2D AnchorOffsets[] =
    {
        FVector2D(Extent.X * 0.10f, 0.0f),
        FVector2D(-Extent.X * 0.10f, 0.0f),
        FVector2D(0.0f, Extent.Y * 0.10f),
        FVector2D(0.0f, -Extent.Y * 0.10f)
    };

    for (const FVector2D& Offset : AnchorOffsets)
    {
        TryAddNavAnchor(FVector(Center.X + Offset.X, Center.Y + Offset.Y, Center.Z), TEXT("Offset"));
    }

    if (NavAnchors.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Key=%s Source=%s Reason=NoNavAnchor AnchorNavFail=%d AnchorBoundsFail=%d Center=%s Extent=%s"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
            AnchorNavFail, AnchorBoundsFail, *Center.ToCompactString(), *Extent.ToCompactString());
        return false;
    }

    float ReferenceNavZ = 0.0f;
    for (const FVector& Anchor : NavAnchors)
    {
        ReferenceNavZ += Anchor.Z;
    }
    ReferenceNavZ /= static_cast<float>(NavAnchors.Num());

    int32 NavFail = 0;
    int32 BoundsFail = 0;
    int32 ZFail = 0;
    int32 NoDropFail = 0;
    int32 DistFail = 0;
    int32 OverlapFail = 0;
    int32 OverheadFail = 0;
    int32 TotalCandidates = 0;

    auto TryAcceptNavLocation = [&](const FNavLocation& NavLocation, const TCHAR* Source, int32 Attempt) -> bool
    {
        ++TotalCandidates;

        if (!DropZone.Bounds.IsInsideXY(NavLocation.Location))
        {
            ++BoundsFail;
            return false;
        }

        if (FVector::DistSquared2D(NavLocation.Location, Center) > FMath::Square(VisibleRadius))
        {
            ++BoundsFail;
            return false;
        }

        const FVector Candidate = NavLocation.Location + FVector(0.0f, 0.0f, CardIslandGroundOffsetZ);

        if (!IsCardDropZSane(DropZone, ReferenceNavZ, Candidate))
        {
            ++ZFail;
            return false;
        }

        if (IsInsideNoDropZone(Candidate))
        {
            ++NoDropFail;
            return false;
        }

        if (!IsFarEnoughFromIslandCards(Candidate, ExistingIslandLocations))
        {
            ++DistFail;
            return false;
        }

        if (!IsCardDropLocationClear(Candidate))
        {
            ++OverlapFail;
            return false;
        }

        if (!HasOverheadClearance(Candidate))
        {
            ++OverheadFail;
            return false;
        }

        OutLocation = Candidate;
        UE_LOG(LogTemp, Warning,
            TEXT("[DS] Card DropInstance Island=%d Slot=%d Zone=%s Key=%s ZoneSource=%s PickSource=%s Attempts=%d TotalCandidates=%d ExistingCards=%d Location=%s SpawnZ=%.1f NavZ=%.1f RefNavZ=%.1f"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
            Source, Attempt, TotalCandidates, ExistingIslandLocations.Num(), *Candidate.ToCompactString(),
            Candidate.Z, NavLocation.Location.Z, ReferenceNavZ);
        return true;
    };

    const float BaseAngleDegrees = 90.0f + static_cast<float>(IslandIndex) * 18.0f;
    const float CandidateRadii[] = { PatternRadius, PatternRadius * 0.72f, PatternRadius * 1.18f };
    int32 PatternAttempt = 0;

    for (float CandidateRadius : CandidateRadii)
    {
        for (int32 Step = 0; Step < 5; ++Step)
        {
            ++PatternAttempt;

            const int32 PatternIndex = (SlotIndex + Step) % 5;
            const float AngleDegrees = BaseAngleDegrees + static_cast<float>(PatternIndex) * 72.0f;
            const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
            const FVector QueryPoint(
                Center.X + FMath::Cos(AngleRadians) * CandidateRadius,
                Center.Y + FMath::Sin(AngleRadians) * CandidateRadius,
                Center.Z);

            FNavLocation NavLocation;
            if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, AnchorProjectExtent))
            {
                ++NavFail;
                continue;
            }

            if (TryAcceptNavLocation(NavLocation, TEXT("NavPattern"), PatternAttempt))
            {
                return true;
            }
        }
    }

    for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
    {
        const FVector& Anchor = NavAnchors[(Attempt + SlotIndex + IslandIndex) % NavAnchors.Num()];

        FNavLocation NavLocation;
        if (!NavSystem->GetRandomReachablePointInRadius(Anchor, RandomRadius, NavLocation))
        {
            ++NavFail;
            continue;
        }

        if (TryAcceptNavLocation(NavLocation, TEXT("NavRandom"), Attempt))
        {
            return true;
        }
    }

    const int32 SpiralRings = 10;
    const int32 PointsPerRing = 16;
    const float GoldenAngleDegrees = 137.50777f;
    int32 SpiralAttempt = 0;

    for (int32 Ring = 1; Ring <= SpiralRings; ++Ring)
    {
        const float RingAlpha = static_cast<float>(Ring) / static_cast<float>(SpiralRings);
        const float RingRadius = FMath::Lerp(MinDistance, RandomRadius, RingAlpha);

        for (int32 PointIndex = 0; PointIndex < PointsPerRing; ++PointIndex)
        {
            ++SpiralAttempt;

            const FVector& Anchor = NavAnchors[(PointIndex + SlotIndex + IslandIndex) % NavAnchors.Num()];
            const float AngleDegrees = GoldenAngleDegrees * static_cast<float>(PointIndex + SlotIndex * 3 + IslandIndex * 7)
                + 360.0f * RingAlpha;
            const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

            const FVector QueryPoint(
                Anchor.X + FMath::Cos(AngleRadians) * RingRadius,
                Anchor.Y + FMath::Sin(AngleRadians) * RingRadius,
                Anchor.Z);

            FNavLocation NavLocation;
            if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, CandidateProjectExtent))
            {
                ++NavFail;
                continue;
            }

            if (TryAcceptNavLocation(NavLocation, TEXT("NavSpiral"), SpiralAttempt))
            {
                return true;
            }
        }
    }

    UE_LOG(LogTemp, Error,
        TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Key=%s ZoneSource=%s Reason=AllNavCandidatesRejected Anchors=%d MaxAttempts=%d SpiralCandidates=%d VisibleRadius=%.0f RandomRadius=%.0f RefNavZ=%.1f NavFail=%d BoundsFail=%d ZFail=%d DistFail=%d OverlapFail=%d OverheadFail=%d NoDrop=%d ExistingCards=%d TotalCandidates=%d"),
        IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
        NavAnchors.Num(), MaxAttempts, SpiralRings * PointsPerRing, VisibleRadius, RandomRadius, ReferenceNavZ,
        NavFail, BoundsFail, ZFail, DistFail, OverlapFail, OverheadFail, NoDropFail, ExistingIslandLocations.Num(), TotalCandidates);
    return false;
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
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

    ACardDropActor* CardActor = GetWorld()->SpawnActor<ACardDropActor>(SpawnClass, SpawnLocation, FRotator::ZeroRotator, Params);
    if (!CardActor)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Card=%d Name=%s Reason=SpawnCollisionOrNull Location=%s"),
            static_cast<int32>(CardID),
            *CardDebug::ToString(CardID),
            *SpawnLocation.ToCompactString());
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
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Reason=NotEnoughDropZones Found=%d Required=%d Result=AbortIslandCardSpawn"),
            IslandDropZones.Num(),
            IslandCardGroups.Num());
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
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandGroup Island=%d Zone=%s Key=%s Source=%s Cards=%d BalanceValue=%d BoundsCenter=%s BoundsExtent=%s"),
            IslandIndex,
            ZoneActor ? *ZoneActor->GetName() : TEXT("None"),
            *DropZone.IslandKey.ToString(),
            *DropZone.Source,
            CardsInIsland.Num(),
            BalanceValue,
            *DropZone.Center.ToString(),
            *DropZone.Bounds.GetExtent().ToString());

        TArray<FVector> ExistingIslandLocations;
        int32 IslandPlaced = 0;

        for (int32 SlotIndex = 0; SlotIndex < CardsInIsland.Num(); ++SlotIndex)
        {
            const ECardID CardID = CardsInIsland[SlotIndex];
            ++PlannedCount;

            FVector SpawnLocation = FVector::ZeroVector;
            const bool bPicked = PickIslandCardDropLocation(DropZone, ExistingIslandLocations, IslandIndex, SlotIndex, SpawnLocation);

            if (!bPicked)
            {
                // 자리를 못 잡으면 겹쳐 놓지 않고 이 카드는 건너뛴다. 실패 사유는 PickIslandCardDropLocation 로그에 남는다.
                UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Card=%d Name=%s Reason=NoValidLocation Result=Skipped"),
                    IslandIndex, SlotIndex, static_cast<int32>(CardID), *CardDebug::ToString(CardID));
                continue;
            }

            ExistingIslandLocations.Add(SpawnLocation);
            ACardDropActor* SpawnedCard = SpawnCardDrop(CardID, SpawnLocation);
            if (SpawnedCard) { ++IslandPlaced; }

            UE_LOG(LogTemp, Warning, TEXT("[DS] Card DropInstanceFinal Island=%d Slot=%d Card=%d Name=%s Result=%s Location=%s SpawnZ=%.1f"),
                IslandIndex, SlotIndex, static_cast<int32>(CardID), *CardDebug::ToString(CardID),
                SpawnedCard ? TEXT("OK") : TEXT("SpawnNull"), *SpawnLocation.ToCompactString(), SpawnLocation.Z);
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandDropSummary Island=%d Zone=%s Key=%s Source=%s Placed=%d Requested=%d Center=%s Extent=%s"),
            IslandIndex, ZoneActor ? *ZoneActor->GetName() : TEXT("None"), *DropZone.IslandKey.ToString(), *DropZone.Source,
            IslandPlaced, CardsInIsland.Num(), *DropZone.Center.ToCompactString(), *DropZone.Bounds.GetExtent().ToCompactString());
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
            AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
            if (GS)
            {
                GS->CurrentRound = CurrentRound;
                GS->OnRep_CurrentRound();
            }
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
        const AMainPlayerState* RedealPS = Pair.Key;
        const FSeotdaPlayerRoundState& RedealState = Pair.Value;

        if (!RedealPS || !RedealState.bSubmitted || RedealState.bFolded)
        {
            continue;
        }

        const bool bIsGusa = RedealState.HandResult.SpecialRule == ESeotdaSpecialRule::Gusa;
        const bool bIsMeongGusa = RedealState.HandResult.SpecialRule == ESeotdaSpecialRule::MeongteongguriGusa;

        if (!bIsGusa && !bIsMeongGusa)
        {
            continue;
        }

        bool bHasOpponent = false;
        FSeotdaHandResult BestOpponentResult;
        FString BestOpponentName = TEXT("None");

        for (const TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& OtherPair : SeotdaRoundStates)
        {
            const AMainPlayerState* OtherPS = OtherPair.Key;
            const FSeotdaPlayerRoundState& OtherState = OtherPair.Value;

            if (!OtherPS || OtherPS == RedealPS || !OtherState.bSubmitted || OtherState.bFolded)
            {
                continue;
            }

            if (!bHasOpponent || CompareSeotdaHands(OtherState.HandResult, BestOpponentResult) > 0)
            {
                bHasOpponent = true;
                BestOpponentResult = OtherState.HandResult;
                BestOpponentName = OtherPS->GetPlayerName();
            }
        }

        if (!bHasOpponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealRule Skip Player=%s Rule=%s Reason=NoActiveOpponent"),
                *RedealPS->GetPlayerName(),
                *RedealState.HandResult.Name);
            continue;
        }

        // ?쇰컲 援ъ궗: ?곷? 理쒓퀬 議깅낫媛 ?뚮━ ?댄븯?대㈃ ?ш꼍湲?
        // 硫띻뎄?? ?곷? 理쒓퀬 議깅낫媛 9???댄븯?대㈃ ?ш꼍湲?
        const int32 AllowedMaxRank = bIsMeongGusa ? 10009 : 9000;
        const bool bAllowRedeal = BestOpponentResult.Rank <= AllowedMaxRank;

        UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealRule Check Player=%s Rule=%s Opponent=%s OpponentCombo=%s OpponentRank=%d AllowedMaxRank=%d Redeal=%d"),
            *RedealPS->GetPlayerName(),
            *RedealState.HandResult.Name,
            *BestOpponentName,
            *BestOpponentResult.Name,
            BestOpponentResult.Rank,
            AllowedMaxRank,
            bAllowRedeal ? 1 : 0);

        if (bAllowRedeal)
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

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
            if (!MPC || MPC->GetPlayerState<AMainPlayerState>() != PS)
            {
                continue;
            }

            const FString RedealNotice = FString::Printf(
                TEXT("[REDEAL] New Cards: #%d:%s, #%d:%s | Combo=%s"),
                FirstRecordId,
                *CardDebug::ToString(FirstInfo.CardID),
                SecondRecordId,
                *CardDebug::ToString(SecondInfo.CardID),
                *State->HandResult.Name
            );

            UE_LOG(LogTemp, Warning, TEXT("[DS] Seotda RedealNotice Player=%s Text=%s"),
                *PS->GetPlayerName(),
                *RedealNotice);

            MPC->Client_ShowSeotdaResult(RedealNotice);
            break;
        }
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
    FVector NewLocation = OffsetTransform.GetLocation() + FVector(0.0f, 0.0f, 200.0f);
    OffsetTransform.SetLocation(NewLocation);
    
    FRotator NewRotation = OffsetTransform.GetRotation().Rotator();
    NewRotation.Yaw += 90.0f;
    OffsetTransform.SetRotation(NewRotation.Quaternion());
    APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OffsetTransform);
    if (NewPlayer && SpawnedPawn)
    {
        NewPlayer->SetControlRotation(NewRotation);
    }
    return SpawnedPawn;

}
