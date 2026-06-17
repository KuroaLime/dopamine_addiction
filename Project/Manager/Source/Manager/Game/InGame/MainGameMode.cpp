#include "Game/InGame/MainGameMode.h"

#include "Game/InGame/PhaseStrategy.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMainGameMode::AMainGameMode()
{
    CurrentStrategy = nullptr;
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

    if (Ticket.IsEmpty())
    {
        ErrorMessage = TEXT("MissingTicket");
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main PreLogin rejected. Error=%s"), *ErrorMessage);
        return;
    }
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
    BroadcastSwitchMode(EGamePhase::Card);
    StartTimedServerPhase(EDediServerPhase::CardGame, GetCardGameDuration());
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
    UE_LOG(LogTemp, Warning, TEXT("[DS] RoundResult Round=%d Winner=%s MoneySummary=%s"),
        CurrentRound,
        TEXT("Pending"),
        TEXT("Pending"));
    StartTimedServerPhase(EDediServerPhase::Result, GetResultDuration());
}

void AMainGameMode::StartTransitionToBattlePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTransitionToBattle after GameEnd Round=%d"), CurrentRound);
        return;
    }

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
    ClearServerPhaseTimer();
    CurrentServerPhase = EDediServerPhase::GameEnd;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);
    SetPlayerPawnGameplayEnabled(false, TEXT("GameEnd"));

    UE_LOG(LogTemp, Warning, TEXT("[DS] GameEnd RoomId=%d Round=%d MaxRound=%d Winner=%s MoneySummary=%s"),
        DediRoomId,
        CurrentRound,
        MaxRoundCount,
        TEXT("Pending"),
        TEXT("Pending"));
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
