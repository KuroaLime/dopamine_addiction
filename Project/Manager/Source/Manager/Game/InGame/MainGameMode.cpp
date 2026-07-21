#include "Game/InGame/MainGameMode.h"
#include "Manager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Containers/StringConv.h"

#include "Game/InGame/PhaseStrategy.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainGameState.h"
#include "Game/InGame/MainCharacter.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "Game/InGame/Card/CardGameService.h"
#include "Game/InGame/TPS/Actor/GoldDropActor.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"

namespace
{
    FString MakePlayerNameFromOptions(const FString& Options)
    {
        FString PlayerName = UGameplayStatics::ParseOption(Options, TEXT("PlayerName"));
        if (PlayerName.IsEmpty())
        {
            PlayerName = UGameplayStatics::ParseOption(Options, TEXT("Name"));
        }

        PlayerName.TrimStartAndEndInline();
        if (PlayerName.IsEmpty())
        {
            return TEXT("");
        }

        FString SafeName;
        SafeName.Reserve(PlayerName.Len());

        for (int32 Index = 0; Index < PlayerName.Len(); ++Index)
        {
            const TCHAR Ch = PlayerName[Index];
            SafeName.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-')) ? Ch : TEXT('_'));
        }

        return SafeName.Left(32);
    }

    FString MainGameModeGetTrimmedEnvValue(const TCHAR* EnvName)
    {
        FString Value;
#ifdef GetEnvironmentVariable
#pragma push_macro("GetEnvironmentVariable")
#undef GetEnvironmentVariable
#define MANAGER_GM_RESTORE_GETENV_MACRO 1
#endif
        Value = FPlatformMisc::GetEnvironmentVariable(EnvName);
#ifdef MANAGER_GM_RESTORE_GETENV_MACRO
#pragma pop_macro("GetEnvironmentVariable")
#undef MANAGER_GM_RESTORE_GETENV_MACRO
#endif
        Value.TrimStartAndEndInline();
        return Value;
    }

    FString MainGameModeResolveOptionOrEnvString(
        const FString& Options,
        const TCHAR* OptionName,
        const TCHAR* PrimaryEnvName,
        const TCHAR* SecondaryEnvName,
        const TCHAR* Fallback)
    {
        FString Value = UGameplayStatics::ParseOption(Options, OptionName);
        Value.TrimStartAndEndInline();
        if (!Value.IsEmpty())
        {
            return Value;
        }

        Value = MainGameModeGetTrimmedEnvValue(PrimaryEnvName);
        if (!Value.IsEmpty())
        {
            return Value;
        }

        Value = MainGameModeGetTrimmedEnvValue(SecondaryEnvName);
        return Value.IsEmpty() ? FString(Fallback) : Value;
    }

    int32 MainGameModeParsePortOrDefault(FString Value, int32 Fallback)
    {
        Value.TrimStartAndEndInline();
        if (Value.IsNumeric())
        {
            const int32 Parsed = FCString::Atoi(*Value);
            if (Parsed > 0 && Parsed <= 65535)
            {
                return Parsed;
            }
        }

        return Fallback;
    }

    int32 MainGameModeResolveOptionOrEnvPort(
        const FString& Options,
        const TCHAR* OptionName,
        const TCHAR* PrimaryEnvName,
        const TCHAR* SecondaryEnvName,
        int32 Fallback)
    {
        FString Value = UGameplayStatics::ParseOption(Options, OptionName);
        Value.TrimStartAndEndInline();
        if (!Value.IsEmpty())
        {
            return MainGameModeParsePortOrDefault(Value, Fallback);
        }

        Value = MainGameModeGetTrimmedEnvValue(PrimaryEnvName);
        if (!Value.IsEmpty())
        {
            return MainGameModeParsePortOrDefault(Value, Fallback);
        }

        Value = MainGameModeGetTrimmedEnvValue(SecondaryEnvName);
        return Value.IsEmpty() ? Fallback : MainGameModeParsePortOrDefault(Value, Fallback);
    }

    const FName& GetPersistentMainWorldLevelName()
    {
        static const FName LevelName(TEXT("Main_Game_World"));
        return LevelName;
    }

    bool IsPersistentMainWorldTarget(FName LevelName)
    {
        return LevelName == GetPersistentMainWorldLevelName();
    }

    constexpr float PositionTeleportMaxLocationError = 50.0f;
    constexpr float PositionTeleportMaxRotationError = 15.0f;

    float GetRotationErrorDegrees(const FRotator& Left, const FRotator& Right)
    {
        const FRotator Delta = (Left - Right).GetNormalized();
        return FMath::Max3(
            FMath::Abs(Delta.Pitch),
            FMath::Abs(Delta.Yaw),
            FMath::Abs(Delta.Roll));
    }

    struct FServerLevelGateState
    {
        bool bPersistentTarget = false;
        bool bWorldBegunPlay = false;
        bool bTargetFound = false;
        bool bTargetLoaded = false;
        bool bTargetVisible = false;
        bool bUnloadComplete = false;
        bool bNavigationSystemPresent = false;
        bool bNavigationBuilding = false;
        bool bReady = false;
    };

    FServerLevelGateState BuildServerLevelGateState(
        UWorld* World,
        FName TargetLevel,
        FName PendingLevelToUnload)
    {
        FServerLevelGateState State;
        State.bPersistentTarget = IsPersistentMainWorldTarget(TargetLevel);
        State.bWorldBegunPlay = World && World->HasBegunPlay();

        if (!World || TargetLevel.IsNone())
        {
            return State;
        }

        if (State.bPersistentTarget)
        {
            const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(World, true);
            State.bTargetFound = FName(*CurrentLevelName) == TargetLevel;
            State.bTargetLoaded = State.bTargetFound && World->PersistentLevel != nullptr && State.bWorldBegunPlay;
            State.bTargetVisible = State.bTargetLoaded;
        }
        else
        {
            ULevelStreaming* StreamingLevel = UGameplayStatics::GetStreamingLevel(World, TargetLevel);
            State.bTargetFound = StreamingLevel != nullptr;
            State.bTargetLoaded = StreamingLevel && StreamingLevel->IsLevelLoaded();
            State.bTargetVisible = StreamingLevel && StreamingLevel->IsLevelVisible();
        }

        ULevelStreaming* StreamingLevelToUnload = PendingLevelToUnload.IsNone()
            || IsPersistentMainWorldTarget(PendingLevelToUnload)
            ? nullptr
            : UGameplayStatics::GetStreamingLevel(World, PendingLevelToUnload);
        State.bUnloadComplete = !StreamingLevelToUnload
            || (!StreamingLevelToUnload->IsLevelLoaded() && !StreamingLevelToUnload->IsLevelVisible());

        UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
        State.bNavigationSystemPresent = NavSystem != nullptr;
        State.bNavigationBuilding = State.bNavigationSystemPresent
            && UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);

        // Dynamic navigation can remain dirty or locked while the world is playable.
        // Keep it in diagnostics, but do not use it as a hard phase/card-spawn gate.
        State.bReady = State.bWorldBegunPlay
            && State.bTargetFound
            && State.bTargetLoaded
            && State.bTargetVisible
            && State.bUnloadComplete;
        return State;
    }
}

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

    const FString ReconnectGraceStr = UGameplayStatics::ParseOption(Options, TEXT("ReconnectGraceSeconds"));
    if (!ReconnectGraceStr.IsEmpty())
    {
        const float ParsedReconnectGraceSeconds = FCString::Atof(*ReconnectGraceStr);
        if (ParsedReconnectGraceSeconds >= 0.0f)
        {
            ReconnectGraceSeconds = ParsedReconnectGraceSeconds;
        }
    }

    const FString DediPortStr = UGameplayStatics::ParseOption(Options, TEXT("DediPort"));
    if (DediPortStr.IsNumeric())
    {
        DediPort = FCString::Atoi(*DediPortStr);
    }

    const FString MatchGenerationStr = UGameplayStatics::ParseOption(Options, TEXT("MatchGeneration"));
    if (MatchGenerationStr.IsNumeric())
    {
        DediMatchGeneration = static_cast<uint32>(FCString::Strtoui64(*MatchGenerationStr, nullptr, 10));
    }

    const FString ControlTokenStr = UGameplayStatics::ParseOption(Options, TEXT("ControlToken"));
    if (ControlTokenStr.IsNumeric())
    {
        DediControlToken = FCString::Strtoui64(*ControlTokenStr, nullptr, 10);
    }

    DediIocpHost = MainGameModeResolveOptionOrEnvString(
        Options,
        TEXT("IocpHost"),
        TEXT("MANAGER_IOCP_CALLBACK_HOST"),
        TEXT("MANAGER_IOCP_HOST"),
        TEXT("127.0.0.1"));
    DediIocpPort = MainGameModeResolveOptionOrEnvPort(
        Options,
        TEXT("IocpPort"),
        TEXT("MANAGER_IOCP_CALLBACK_PORT"),
        TEXT("MANAGER_IOCP_PORT"),
        9000);

    AllowedDediTickets.Empty();
    ActiveDediTickets.Empty();
    PendingDediTicketReservations.Empty();
    DediTicketByController.Empty();
    DisconnectedPlayerSnapshots.Empty();
    MatchParticipantGoldLedger.Empty();
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReconnectGraceTimerHandle);
    }

    const FString TicketsStr = UGameplayStatics::ParseOption(Options, TEXT("Tickets"));
    if (!TicketsStr.IsEmpty())
    {
        TArray<FString> TicketParts;
        TicketsStr.ParseIntoArray(TicketParts, TEXT(","), true);

        for (FString TicketPart : TicketParts)
        {
            TicketPart.TrimStartAndEndInline();
            if (!TicketPart.IsNumeric())
            {
                DS_LOG(TEXT("[DS] Main InitGame ignored invalid ticket option. Value=%s RoomId=%d"),
                    *TicketPart,
                    DediRoomId);
                continue;
            }

            const int64 TicketValue = FCString::Atoi64(*TicketPart);
            if (TicketValue > 0)
            {
                AllowedDediTickets.Add(TicketValue);
            }
        }
    }

    DS_LOG(TEXT("[DS] Main InitGame Map=%s RoomId=%d DediPort=%d Generation=%u ControlTokenPresent=%d Iocp=%s:%d RequiredPlayers=%d AllowedTickets=%d"),
        *MapName,
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken != 0 ? 1 : 0,
        *DediIocpHost,
        DediIocpPort,
        RequiredPlayerCount,
        AllowedDediTickets.Num());
}

void AMainGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    FString Ticket = UGameplayStatics::ParseOption(Options, TEXT("ticket"));
    Ticket.TrimStartAndEndInline();

    DS_LOG(TEXT("[DS] Main PreLogin Address=%s TicketPresent=%d"),
        *Address,
        Ticket.IsEmpty() ? 0 : 1);

    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        DS_LOG(TEXT("[DS] Main PreLogin rejected by Super. Error=%s"), *ErrorMessage);
        return;
    }

    SweepExpiredDediTicketReservations(TEXT("PreLogin"));

    const bool bRequireTicket = DediRoomId > 0;
    if (bRequireTicket && Ticket.IsEmpty())
    {
        ErrorMessage = TEXT("MissingTicket");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d"),
            *ErrorMessage,
            *Address,
            DediRoomId);
        return;
    }

    int64 TicketValue = 0;
    if (!Ticket.IsEmpty() && Ticket.IsNumeric())
    {
        TicketValue = FCString::Atoi64(*Ticket);
    }

    if (bRequireTicket && (!Ticket.IsNumeric() || TicketValue <= 0))
    {
        ErrorMessage = TEXT("InvalidTicket");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d Ticket=%s"),
            *ErrorMessage,
            *Address,
            DediRoomId,
            *Ticket);
        return;
    }

    if (bRequireTicket && AllowedDediTickets.Num() == 0)
    {
        ErrorMessage = TEXT("MissingTicketList");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d Ticket=%s"),
            *ErrorMessage,
            *Address,
            DediRoomId,
            *Ticket);
        return;
    }

    if (bRequireTicket && !AllowedDediTickets.Contains(TicketValue))
    {
        ErrorMessage = TEXT("UnknownTicket");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d Ticket=%s"),
            *ErrorMessage,
            *Address,
            DediRoomId,
            *Ticket);
        return;
    }

    if (bRequireTicket &&
        (ActiveDediTickets.Contains(TicketValue) || PendingDediTicketReservations.Contains(TicketValue)))
    {
        ErrorMessage = TEXT("TicketAlreadyActive");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d"),
            *ErrorMessage,
            *Address,
            DediRoomId);
        return;
    }

    if (bRequireTicket && IsReconnectGraceExpired(TicketValue))
    {
        ExpireDisconnectedPlayerSnapshot(TicketValue, TEXT("PreLoginGraceExpired"));
        ErrorMessage = TEXT("ReconnectGraceExpired");
        DS_LOG(TEXT("[DS] Main PreLogin rejected. Error=%s Address=%s RoomId=%d Ticket=%s"),
            *ErrorMessage,
            *Address,
            DediRoomId,
            *Ticket);
        return;
    }

    if (bRequireTicket)
    {
        PendingDediTicketReservations.Add(
            TicketValue,
            FPlatformTime::Seconds() + FMath::Max(5.0f, DediTicketReservationTimeoutSeconds));
    }
}

FString AMainGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options,
    const FString& Portal)
{
    FString TicketText = UGameplayStatics::ParseOption(Options, TEXT("ticket"));
    TicketText.TrimStartAndEndInline();
    const int64 TicketValue = TicketText.IsNumeric() ? FCString::Atoi64(*TicketText) : 0;

    const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    SweepExpiredDediTicketReservations(TEXT("InitNewPlayer"));
    if (!Result.IsEmpty())
    {
        if (TicketValue > 0)
        {
            PendingDediTicketReservations.Remove(TicketValue);
            ActiveDediTickets.Remove(TicketValue);
        }
        return Result;
    }

    if (DediRoomId > 0 &&
        (!NewPlayerController ||
            TicketValue <= 0 ||
            !PendingDediTicketReservations.Contains(TicketValue)))
    {
        PendingDediTicketReservations.Remove(TicketValue);
        ActiveDediTickets.Remove(TicketValue);
        UE_LOG(LogManager, Error,
            TEXT("[DS] Main InitNewPlayer rejected. Error=TicketReservationMissing Controller=%s Ticket=%lld"),
            *GetNameSafe(NewPlayerController),
            TicketValue);
        return TEXT("TicketReservationMissing");
    }

    if (NewPlayerController && TicketValue > 0)
    {
        PendingDediTicketReservations.Remove(TicketValue);
        ActiveDediTickets.Add(TicketValue);
        DediTicketByController.Add(NewPlayerController, TicketValue);
    }

    const FString PlayerName = MakePlayerNameFromOptions(Options);

    AMainPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<AMainPlayerState>() : nullptr;
    if (PS && !PlayerName.IsEmpty())
    {
        PS->SetPlayerName(PlayerName);
        DS_LOG(TEXT("[DS] Main InitNewPlayer PlayerName=%s Controller=%s TicketBound=%d"),
            *PlayerName,
            NewPlayerController ? *NewPlayerController->GetName() : TEXT("<NULL>"),
            TicketValue > 0 ? 1 : 0);
    }
    else
    {
        DS_LOG(TEXT("[DS] Main InitNewPlayer PlayerNameFallback CurrentName=%s Controller=%s TicketBound=%d"),
            PS ? *PS->GetPlayerName() : TEXT("<NO_PLAYER_STATE>"),
            NewPlayerController ? *NewPlayerController->GetName() : TEXT("<NULL>"),
            TicketValue > 0 ? 1 : 0);
    }

    return Result;
}

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    InitStrategy();

    // 카드게임 도메인 서비스 생성/주입(상태+로직 소유). 서버 전용.
    CardGameService = NewObject<UCardGameService>(this);
    CardGameService->Init(this);

    DS_LOG(TEXT("[DS] Main BeginPlay RoomId=%d RequiredPlayers=%d InitialPhase=%d Strategies=%d DebugPhase=%d RealReady=%d RealBattle=%d RealTransition=%d RealCard=%d RealResult=%d DebugReady=%d DebugBattle=%d DebugTransition=%d DebugCard=%d DebugResult=%d"),
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

    NotifyIocpServerReady();

    if (DediRoomId > 0 && DediPort > 0 && DediMatchGeneration != 0 && DediControlToken != 0)
    {
        BeginDediRecoveryWatchdogStage(TEXT("WaitingPlayers"), TEXT("BeginPlay"));
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                DediRecoveryWatchdogTimerHandle,
                this,
                &AMainGameMode::TickDediRecoveryWatchdog,
                FMath::Max(0.1f, DediRecoveryWatchdogIntervalSeconds),
                true);
        }
    }
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    const int64 Ticket = GetDediTicketForController(NewPlayer);
    RestoreDisconnectedPlayerSnapshot(NewPlayer, Ticket);

    if (bGameStarted)
    {
        AMainPlayerState* PS = NewPlayer
            ? NewPlayer->GetPlayerState<AMainPlayerState>()
            : nullptr;
        if (PS)
        {
            RecordMatchParticipantGold(
                Ticket,
                PS->GetPlayerName(),
                PS->CurPlayerData.HoldingGold);
        }

        if (PS && CardGameService && CurrentServerPhase == EDediServerPhase::CardGame)
        {
            const bool bCardsReady = CardGameService->EnsureCardsForReconnectedPlayer(
                PS,
                TEXT("PostLoginCardPhase"));
            DS_LOG(TEXT("[DS] Reconnect CardCatchUpPostLogin Ticket=%lld Player=%s Ready=%d Count=%d Round=%d"),
                Ticket,
                *PS->GetPlayerName(),
                bCardsReady ? 1 : 0,
                PS->OwnedCards.Num(),
                CurrentRound);
        }

        SynchronizePlayerWithCurrentServerPhase(Cast<AMainPlayerController>(NewPlayer));
    }

    DS_LOG(TEXT("[DS] Main PostLogin Controller=%s RoomId=%d HumanPlayers=%d/%d"),
        NewPlayer ? *NewPlayer->GetName() : TEXT("<NULL>"),
        DediRoomId,
        CountConnectedHumanPlayers(),
        RequiredPlayerCount);

    MarkDediRecoveryProgress(TEXT("PostLogin"));
    TryStartGameIfReady();
}

void AMainGameMode::Logout(AController* Exiting)
{
    DS_LOG(TEXT("[DS] Main Logout Controller=%s RoomId=%d HumanPlayersBeforeSuper=%d/%d"),
        Exiting ? *Exiting->GetName() : TEXT("<NULL>"),
        DediRoomId,
        CountConnectedHumanPlayers(),
        RequiredPlayerCount);

    const bool bReturningAfterMatchEnd = bGameEndReached && !bMatchAbortRequested;
    const int32 RemainingPlayersAfterLogout = FMath::Max(
        0,
        CountConnectedHumanPlayers() - (Exiting ? 1 : 0));

    const int64 Ticket = GetDediTicketForController(Exiting);
    if (!bGameEndReached)
    {
        SaveDisconnectedPlayerSnapshot(Exiting, Ticket);
    }

    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(Exiting))
    {
        const TWeakObjectPtr<AMainPlayerController> PlayerKey(MainPC);
        ClientLoadedStreamLevels.Remove(PlayerKey);
        ClientLoadedStreamPhases.Remove(PlayerKey);
        ClientRequestedStreamLevels.Remove(PlayerKey);
        ClientStreamRequestTimes.Remove(PlayerKey);
        ClientStreamGateExcludedReasons.Remove(PlayerKey);
        ClientReadyCardBundleGenerations.Remove(PlayerKey);
    }

    if (APlayerController* ExitingPC = Cast<APlayerController>(Exiting))
    {
        DediTicketByController.Remove(ExitingPC);
    }
    if (Ticket > 0)
    {
        ActiveDediTickets.Remove(Ticket);
    }

    Super::Logout(Exiting);

    if (CardGameService)
    {
        CardGameService->HandlePlayerDisconnectedAfterLogout(Ticket, TEXT("Logout"));
    }

    if (bReturningAfterMatchEnd && RemainingPlayersAfterLogout <= 0)
    {
        FinalizeMatchEndAndShutdown(TEXT("AllClientsReturned"));
    }

    TrySpawnBattleRoyaleCardsWhenStreamReady();

    DS_LOG(TEXT("[DS] Main Logout Complete HumanPlayers=%d/%d GameStarted=%d Phase=%s Round=%d"),
        CountConnectedHumanPlayers(),
        RequiredPlayerCount,
        bGameStarted ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound);
}

void AMainGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    const int64 Ticket = GetDediTicketForController(NewPlayer);
    FDisconnectedPlayerSnapshot* Snapshot = DisconnectedPlayerSnapshots.Find(Ticket);
    AMainPlayerController* MainPC = Cast<AMainPlayerController>(NewPlayer);
    if (Snapshot &&
        Snapshot->bPlayerStateRestored &&
        Snapshot->bHasPawnTransform &&
        MainPC)
    {
        Snapshot->ReconnectedController = MainPC;
        if (MainPC->GetPawn() &&
            TeleportPlayerAuthoritatively(MainPC, Snapshot->PawnTransform, TEXT("ReconnectStart")))
        {
            DisconnectedPlayerSnapshots.Remove(Ticket);
            StopReconnectGraceTimerIfIdle();
        }
        else
        {
            StartReconnectGraceTimerIfNeeded();
        }
    }
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
        DS_LOG(TEXT("[DS] Main BeginPhase failed. Missing strategy phase=%d Round=%d ServerPhase=%s"),
            static_cast<int32>(CurrPhase),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    DS_LOG(TEXT("[DS] Main BeginPhase phase=%d strategy=%s Round=%d ServerPhase=%s"),
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
        DS_LOG(TEXT("[DS] Main EndPhase strategy=%s Round=%d ServerPhase=%s"),
            *CurrentStrategy->GetName(),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        CurrentStrategy->OnPhaseEnd();
        CurrentStrategy = nullptr;
    }
}

void AMainGameMode::ChangePhase(EGamePhase NewPhase)
{
    DS_LOG(TEXT("[DS] Main ChangePhase ignored newPhase=%d Round=%d CurrentServerPhase=%s GameEnd=%d"),
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

    DS_LOG(TEXT("[DS] Main BroadcastSwitchMode phase=%d targets=%d Round=%d ServerPhase=%s"),
        static_cast<int32>(NewPhase),
        TargetCount,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::BroadcastSwitchLevel(FName LevelToUnload, FName LevelToLoad)
{
    ResetClientStreamLevelAcks(TEXT("BroadcastSwitchLevel"));
    UnloadServerStreamLevelForPhase(LevelToUnload, TEXT("BroadcastSwitchLevel"));
    LoadServerStreamLevelForPhase(LevelToLoad, TEXT("BroadcastSwitchLevel"));

    int32 TargetCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(It->Get()))
        {
            PC->SwitchToLevel(LevelToUnload, LevelToLoad);
            RegisterClientStreamLevelRequest(Cast<AMainPlayerController>(It->Get()), LevelToLoad, TEXT("BroadcastSwitchLevel"));
            TargetCount++;
        }
    }

    DS_LOG(TEXT("[DS] Main BroadcastSwitchLevel unload=%s load=%s targets=%d Round=%d ServerPhase=%s"),
        *LevelToUnload.ToString(),
        *LevelToLoad.ToString(),
        TargetCount,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::UnloadServerStreamLevelForPhase(FName LevelToUnload, const TCHAR* Context)
{
    PendingServerStreamLevelToUnload = NAME_None;

    if (LevelToUnload.IsNone() || IsPersistentMainWorldTarget(LevelToUnload))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    ULevelStreaming* StreamingLevel = UGameplayStatics::GetStreamingLevel(World, LevelToUnload);
    if (!StreamingLevel || (!StreamingLevel->IsLevelLoaded() && !StreamingLevel->IsLevelVisible()))
    {
        DS_LOG(TEXT("[DS] Main ServerUnloadStreamLevel already-complete unload=%s Context=%s Round=%d ServerPhase=%s"),
            *LevelToUnload.ToString(),
            Context ? Context : TEXT("<NULL>"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    PendingServerStreamLevelToUnload = LevelToUnload;

    FLatentActionInfo UnloadInfo;
    UnloadInfo.CallbackTarget = this;
    UnloadInfo.UUID = ++ServerStreamingLatentActionId;
    UGameplayStatics::UnloadStreamLevel(World, LevelToUnload, UnloadInfo, false);

    DS_LOG(TEXT("[DS] Main ServerUnloadStreamLevel unload=%s Context=%s Round=%d ServerPhase=%s UUID=%d"),
        *LevelToUnload.ToString(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        UnloadInfo.UUID);
}

void AMainGameMode::LoadServerStreamLevelForPhase(FName LevelToLoad, const TCHAR* Context)
{
    if (LevelToLoad.IsNone())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    if (IsPersistentMainWorldTarget(LevelToLoad))
    {
        DS_LOG(TEXT("[DS] Main ServerPersistentLevelTarget level=%s Current=%s Context=%s Round=%d ServerPhase=%s"),
            *LevelToLoad.ToString(),
            *UGameplayStatics::GetCurrentLevelName(World, true),
            Context ? Context : TEXT("<NULL>"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    FLatentActionInfo LoadInfo;
    LoadInfo.CallbackTarget = this;
    LoadInfo.UUID = ++ServerStreamingLatentActionId;

    UGameplayStatics::LoadStreamLevel(World, LevelToLoad, true, true, LoadInfo);
    World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

    DS_LOG(TEXT("[DS] Main ServerLoadStreamLevel load=%s Context=%s Round=%d ServerPhase=%s UUID=%d"),
        *LevelToLoad.ToString(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        LoadInfo.UUID);
}

bool AMainGameMode::IsBattleRoyalePhase() const
{
    return bGameStarted
        && CurrentServerPhase == EDediServerPhase::BattleRoyale
        && bBattleRoyaleCardsSpawnedThisPhase;
}

bool AMainGameMode::TryRespawnPlayerAuthoritatively(AMainCharacter* Character, const TCHAR* Context)
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !World || !IsValid(Character))
    {
        return false;
    }

    AMainPlayerController* MainPC = Cast<AMainPlayerController>(Character->GetController());
    AMainPlayerState* PlayerState = Character->GetPlayerState<AMainPlayerState>();
    if (!MainPC || !PlayerState)
    {
        UE_LOG(LogManager, Warning,
            TEXT("[DS] RespawnRetry Reason=MissingControllerOrPlayerState Character=%s Context=%s"),
            *GetNameSafe(Character),
            Context ? Context : TEXT("<NULL>"));
        return false;
    }

    // A delayed death timer may complete after the round has already left combat.
    // Restore health, but let the current phase strategy own visibility, input and placement.
    if (!IsBattleRoyalePhase())
    {
        PlayerState->ResetState();
        DS_LOG(TEXT("[DS] RespawnNormalizedOutsideBattle Player=%s Phase=%s Context=%s"),
            *GetNameSafe(PlayerState),
            GetServerPhaseName(CurrentServerPhase),
            Context ? Context : TEXT("<NULL>"));
        return true;
    }

    USpawnManagerComponent* ActiveSpawnManager = USpawnManagerComponent::GetActive(this);
    AA_Spawn* SpawnPoint = ActiveSpawnManager ? ActiveSpawnManager->GetRandomCenterSpawnActor() : nullptr;
    if (!IsValid(SpawnPoint))
    {
        return false;
    }

    FTransform SpawnTransform = SpawnPoint->GetActorTransform();
    SpawnTransform.AddToTranslation(FVector(0.f, 0.f, 200.f));
    FRotator SpawnRotation = SpawnTransform.GetRotation().Rotator();
    SpawnRotation.Yaw += 90.f;
    SpawnTransform.SetRotation(SpawnRotation.Quaternion());

    // HP and controls are restored only after the authoritative placement succeeded.
    if (!TeleportPlayerAuthoritatively(MainPC, SpawnTransform, Context))
    {
        return false;
    }

    PlayerState->ResetState();
    Character->UnCrouch();
    Character->SetReplicateMovement(true);
    Character->SetActorHiddenInGame(false);
    Character->SetActorEnableCollision(true);

    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        MoveComp->SetBase(nullptr);
        MoveComp->StopMovementImmediately();
        MoveComp->SetMovementMode(MOVE_Walking);
    }

    if (AWeapon* EquippedWeapon = Character->GetEquippedGun())
    {
        EquippedWeapon->SetActorHiddenInGame(false);
        EquippedWeapon->SetActorEnableCollision(true);
        EquippedWeapon->SetActorTickEnabled(true);
        if (EquippedWeapon->Setting)
        {
            EquippedWeapon->Setting->CancelReloadLock();
        }
        EquippedWeapon->ForceNetUpdate();
    }

    Character->ForceNetUpdate();
    MainPC->SetGameplayInputLocked(false, Context);
    MainPC->SwitchState(EGamePhase::TPS);

    DS_LOG(TEXT("[DS] RespawnComplete Player=%s Spawn=%s Location=%s Context=%s"),
        *GetNameSafe(PlayerState),
        *GetNameSafe(SpawnPoint),
        *Character->GetActorLocation().ToString(),
        Context ? Context : TEXT("<NULL>"));
    return true;
}

bool AMainGameMode::IsShopRequestAllowed() const
{
    if (!bGameStarted || CurrentServerPhase != EDediServerPhase::PreBattleShop)
    {
        return false;
    }

    const AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    return GS && GS->IsShopAvailable();
}

bool AMainGameMode::DropGoldFromPlayer(AMainPlayerState* TargetPS, AActor* SourceActor)
{
    if (!HasAuthority() || !GetWorld() || !TargetPS || !SourceActor || !IsBattleRoyalePhase())
    {
        return false;
    }

    const int32 AvailableGold = FMath::Max(0, TargetPS->CurPlayerData.HoldingGold);
    const int32 DropAmount = FMath::Min(AvailableGold, FMath::Max(0, DeathGoldDropAmount));
    if (DropAmount <= 0)
    {
        DS_LOG(TEXT("[DS] GoldDrop Skip Player=%s Reason=NoGold Available=%d Configured=%d"),
            *TargetPS->GetPlayerName(),
            AvailableGold,
            DeathGoldDropAmount);
        return false;
    }

    const FVector SourceLocation = SourceActor->GetActorLocation();
    FVector DropLocation = SourceLocation + FVector(0.0f, 0.0f, DeathGoldGroundOffsetZ);

    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DeathGoldGround), false);
    QueryParams.AddIgnoredActor(SourceActor);
    if (GetWorld()->LineTraceSingleByChannel(
        GroundHit,
        SourceLocation + FVector(0.0f, 0.0f, 200.0f),
        SourceLocation - FVector(0.0f, 0.0f, FMath::Max(100.0f, DeathGoldGroundTraceDepth)),
        ECC_Visibility,
        QueryParams))
    {
        DropLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, DeathGoldGroundOffsetZ);
    }

    TSubclassOf<AGoldDropActor> SpawnClass = GoldDropActorClass;
    if (!SpawnClass)
    {
        SpawnClass = AGoldDropActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.Owner = SourceActor;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AGoldDropActor* GoldDrop = GetWorld()->SpawnActor<AGoldDropActor>(
        SpawnClass,
        DropLocation,
        FRotator::ZeroRotator,
        Params);
    if (!GoldDrop)
    {
        UE_LOG(LogManager, Error,
            TEXT("[DS] GoldDrop SpawnFail Player=%s Amount=%d Requested=%s"),
            *TargetPS->GetPlayerName(),
            DropAmount,
            *DropLocation.ToCompactString());
        return false;
    }

    TargetPS->AddGold(-DropAmount);
    ActiveGoldDrops.RemoveAll([](const TObjectPtr<AGoldDropActor>& ExistingDrop)
    {
        return !IsValid(ExistingDrop);
    });
    ActiveGoldDrops.Add(GoldDrop);
    GoldDrop->InitGoldDrop(
        DropAmount,
        TargetPS,
        DeathGoldSourcePickupLockSeconds);

    UE_LOG(LogManager, Display,
        TEXT("[DS] GoldDrop Spawned Player=%s Amount=%d Gold=%d->%d Requested=%s Actual=%s"),
        *TargetPS->GetPlayerName(),
        DropAmount,
        AvailableGold,
        TargetPS->CurPlayerData.HoldingGold,
        *DropLocation.ToCompactString(),
        *GoldDrop->GetActorLocation().ToCompactString());
    return true;
}
bool AMainGameMode::IsPreBattleShopPhase() const
{
    return bGameStarted && CurrentServerPhase == EDediServerPhase::PreBattleShop;
}
void AMainGameMode::InitStrategy()
{
    StrategyMap.Empty();

    for (auto& Pair : StrategyClassMap)
    {
        if (!Pair.Value)
        {
            DS_LOG(TEXT("[DS] Main InitStrategy skipped null phase=%d"), static_cast<int32>(Pair.Key));
            continue;
        }

        UPhaseStrategy* Strategy = NewObject<UPhaseStrategy>(this, Pair.Value);
        if (Strategy)
        {
            Strategy->Initialize(this);
            StrategyMap.Add(Pair.Key, Strategy);

            DS_LOG(TEXT("[DS] Main InitStrategy phase=%d strategy=%s"),
                static_cast<int32>(Pair.Key),
                *Strategy->GetName());
        }
    }
}

void AMainGameMode::TryStartGameIfReady()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] Main TryStart ignored. GameEnd reached RoomId=%d Round=%d"), DediRoomId, CurrentRound);
        return;
    }

    if (bGameStarted)
    {
        return;
    }

    const int32 HumanPlayers = CountConnectedHumanPlayers();
    if (HumanPlayers < RequiredPlayerCount)
    {
        DS_LOG(TEXT("[DS] Main WaitingPlayers HumanPlayers=%d/%d"),
            HumanPlayers,
            RequiredPlayerCount);
        return;
    }

    ClearDediRecoveryWatchdogStage(TEXT("RequiredPlayersReady"));
    DediEmptySinceSeconds = 0.0;
    bGameStarted = true;

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PlayerController = It->Get();
            const AMainPlayerState* PS = PlayerController
                ? PlayerController->GetPlayerState<AMainPlayerState>()
                : nullptr;
            if (PlayerController && PS)
            {
                RecordMatchParticipantGold(
                    GetDediTicketForController(PlayerController),
                    PS->GetPlayerName(),
                    PS->CurPlayerData.HoldingGold);
            }
        }
    }

    CurrentRound = 1;
    AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    if (GS)
    {
        GS->CurrentRound = CurrentRound;
        GS->OnRep_CurrentRound();
    }
    DS_LOG(TEXT("[DS] Main RequiredPlayersReady HumanPlayers=%d/%d StartRound=%d MaxRound=%d"),
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

int64 AMainGameMode::GetDediTicketForController(const AController* Controller) const
{
    const APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return 0;
    }

    const int64* Ticket = DediTicketByController.Find(const_cast<APlayerController*>(PlayerController));
    return Ticket ? *Ticket : 0;
}

void AMainGameMode::RecordMatchParticipantGold(
    int64 Ticket,
    const FString& PlayerName,
    int32 Gold)
{
    if (Ticket <= 0)
    {
        return;
    }

    FMatchParticipantGoldRecord& Record = MatchParticipantGoldLedger.FindOrAdd(Ticket);
    Record.PlayerName = PlayerName.IsEmpty()
        ? FString::Printf(TEXT("Player_%lld"), Ticket)
        : PlayerName;
    Record.Gold = FMath::Max(0, Gold);
}

void AMainGameMode::SweepExpiredDediTicketReservations(const TCHAR* Context)
{
    const double NowSeconds = FPlatformTime::Seconds();
    TArray<int64> ExpiredTickets;
    for (const TPair<int64, double>& Pair : PendingDediTicketReservations)
    {
        if (Pair.Value > 0.0 && NowSeconds >= Pair.Value)
        {
            ExpiredTickets.Add(Pair.Key);
        }
    }

    for (int64 Ticket : ExpiredTickets)
    {
        PendingDediTicketReservations.Remove(Ticket);
        UE_LOG(LogManager, Warning,
            TEXT("[DS] DediTicket ReservationExpired Ticket=%lld Context=%s"),
            Ticket,
            Context ? Context : TEXT("<NULL>"));
    }
}

void AMainGameMode::SaveDisconnectedPlayerSnapshot(AController* Exiting, int64 Ticket)
{
    if (!Exiting || Ticket <= 0)
    {
        return;
    }

    AMainPlayerState* PlayerState = Exiting->GetPlayerState<AMainPlayerState>();
    if (!PlayerState)
    {
        return;
    }

    FDisconnectedPlayerSnapshot Snapshot;
    PlayerState->CaptureReconnectSnapshot(Snapshot.PlayerState);
    Snapshot.PlayerName = PlayerState->GetPlayerName();
    Snapshot.DisconnectTimeSeconds = FPlatformTime::Seconds();
    Snapshot.ReconnectDeadlineSeconds = Snapshot.DisconnectTimeSeconds + FMath::Max(0.0f, ReconnectGraceSeconds);
    Snapshot.DisconnectRound = CurrentRound;
    Snapshot.DisconnectPhase = CurrentServerPhase;

    if (bGameStarted)
    {
        RecordMatchParticipantGold(
            Ticket,
            PlayerState->GetPlayerName(),
            PlayerState->CurPlayerData.HoldingGold);
    }

    if (APawn* Pawn = Exiting->GetPawn())
    {
        Snapshot.PawnTransform = Pawn->GetActorTransform();
        Snapshot.bHasPawnTransform = true;
    }

    if (CardGameService)
    {
        CardGameService->DetachPlayerForReconnect(Ticket, PlayerState);
    }

    DisconnectedPlayerSnapshots.Add(Ticket, MoveTemp(Snapshot));
    StartReconnectGraceTimerIfNeeded();

    DS_LOG(TEXT("[DS] Reconnect SnapshotSaved Ticket=%lld Player=%s HasTransform=%d Phase=%s Round=%d Grace=%.1f Deadline=%.3f"),
        Ticket,
        *PlayerState->GetPlayerName(),
        DisconnectedPlayerSnapshots[Ticket].bHasPawnTransform ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound,
        ReconnectGraceSeconds,
        DisconnectedPlayerSnapshots[Ticket].ReconnectDeadlineSeconds);
}

void AMainGameMode::RestoreDisconnectedPlayerSnapshot(APlayerController* NewPlayer, int64 Ticket)
{
    FDisconnectedPlayerSnapshot* Snapshot = DisconnectedPlayerSnapshots.Find(Ticket);
    AMainPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AMainPlayerState>() : nullptr;
    if (!Snapshot || !PlayerState || Ticket <= 0)
    {
        return;
    }

    if (IsReconnectGraceExpired(Ticket))
    {
        ExpireDisconnectedPlayerSnapshot(Ticket, TEXT("RestoreGraceExpired"));
        return;
    }

    PlayerState->RestoreReconnectSnapshot(Snapshot->PlayerState);
    Snapshot->bPlayerStateRestored = true;
    Snapshot->TransformRestoreDeadlineSeconds = FPlatformTime::Seconds() +
        FMath::Max(5.0f, ReconnectTransformRestoreTimeoutSeconds);

    if (CardGameService)
    {
        CardGameService->ReattachPlayerAfterReconnect(Ticket, PlayerState);
    }

    bool bTransformRestored = !Snapshot->bHasPawnTransform;
    if (Snapshot->bHasPawnTransform)
    {
        if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(NewPlayer))
        {
            Snapshot->ReconnectedController = MainPC;
            if (MainPC->GetPawn())
            {
                ++Snapshot->TransformRestoreAttempts;
                bTransformRestored = TeleportPlayerAuthoritatively(
                    MainPC,
                    Snapshot->PawnTransform,
                    TEXT("ReconnectPostLogin"));
            }
        }
    }

    DS_LOG(TEXT("[DS] Reconnect SnapshotRestored Ticket=%lld Player=%s TransformRestored=%d Phase=%s Round=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        bTransformRestored ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound);

    if (bTransformRestored)
    {
        DisconnectedPlayerSnapshots.Remove(Ticket);
    }
    else
    {
        StartReconnectGraceTimerIfNeeded();
    }
    StopReconnectGraceTimerIfIdle();
}

bool AMainGameMode::IsReconnectGraceExpired(int64 Ticket) const
{
    const FDisconnectedPlayerSnapshot* Snapshot = DisconnectedPlayerSnapshots.Find(Ticket);
    if (!Snapshot || Snapshot->bPlayerStateRestored)
    {
        return false;
    }

    return Snapshot->ReconnectDeadlineSeconds > 0.0 &&
        FPlatformTime::Seconds() >= Snapshot->ReconnectDeadlineSeconds;
}

void AMainGameMode::ExpireDisconnectedPlayerSnapshot(int64 Ticket, const TCHAR* Reason)
{
    FDisconnectedPlayerSnapshot RemovedSnapshot;
    const bool bHadSnapshot = DisconnectedPlayerSnapshots.RemoveAndCopyValue(Ticket, RemovedSnapshot);

    if (bGameStarted && bHadSnapshot)
    {
        RecordMatchParticipantGold(
            Ticket,
            RemovedSnapshot.PlayerName,
            RemovedSnapshot.PlayerState.CurPlayerData.HoldingGold);
    }

    if (CardGameService)
    {
        CardGameService->ExpireReconnectState(Ticket, Reason);
    }

    AllowedDediTickets.Remove(Ticket);
    ActiveDediTickets.Remove(Ticket);
    PendingDediTicketReservations.Remove(Ticket);

    DS_LOG(TEXT("[DS] Reconnect GraceExpired Ticket=%lld HadSnapshot=%d DisconnectPhase=%s DisconnectRound=%d Reason=%s"),
        Ticket,
        bHadSnapshot ? 1 : 0,
        bHadSnapshot ? GetServerPhaseName(RemovedSnapshot.DisconnectPhase) : TEXT("None"),
        bHadSnapshot ? RemovedSnapshot.DisconnectRound : 0,
        Reason ? Reason : TEXT("<NULL>"));

    StopReconnectGraceTimerIfIdle();
}

void AMainGameMode::StartReconnectGraceTimerIfNeeded()
{
    if (!GetWorld() ||
        DisconnectedPlayerSnapshots.IsEmpty() ||
        GetWorld()->GetTimerManager().IsTimerActive(ReconnectGraceTimerHandle))
    {
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        ReconnectGraceTimerHandle,
        this,
        &AMainGameMode::TickReconnectGrace,
        1.0f,
        true);
}

void AMainGameMode::StopReconnectGraceTimerIfIdle()
{
    if (!GetWorld())
    {
        return;
    }

    if (!DisconnectedPlayerSnapshots.IsEmpty())
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(ReconnectGraceTimerHandle);
}

void AMainGameMode::TickReconnectGrace()
{
    TArray<int64> ExpiredTickets;
    TArray<int64> CompletedTickets;
    const double NowSeconds = FPlatformTime::Seconds();

    for (TPair<int64, FDisconnectedPlayerSnapshot>& Pair : DisconnectedPlayerSnapshots)
    {
        FDisconnectedPlayerSnapshot& Snapshot = Pair.Value;
        if (!Snapshot.bPlayerStateRestored)
        {
            if (IsReconnectGraceExpired(Pair.Key))
            {
                ExpiredTickets.Add(Pair.Key);
            }
            continue;
        }

        if (!Snapshot.bHasPawnTransform)
        {
            CompletedTickets.Add(Pair.Key);
            continue;
        }

        AMainPlayerController* ReconnectedPC = Snapshot.ReconnectedController.Get();
        if (ReconnectedPC && ReconnectedPC->GetPawn())
        {
            ++Snapshot.TransformRestoreAttempts;
            if (TeleportPlayerAuthoritatively(
                ReconnectedPC,
                Snapshot.PawnTransform,
                TEXT("ReconnectTransformRetry")))
            {
                DS_LOG(TEXT("[DS] Reconnect TransformRetryComplete Ticket=%lld Attempts=%d Player=%s"),
                    Pair.Key,
                    Snapshot.TransformRestoreAttempts,
                    *GetNameSafe(ReconnectedPC->PlayerState));
                CompletedTickets.Add(Pair.Key);
                continue;
            }
        }

        if (Snapshot.TransformRestoreDeadlineSeconds > 0.0 &&
            NowSeconds >= Snapshot.TransformRestoreDeadlineSeconds)
        {
            UE_LOG(LogManager, Error,
                TEXT("[DS] Reconnect TransformRestoreAbandoned Ticket=%lld Attempts=%d Player=%s HasController=%d HasPawn=%d Action=KeepCurrentServerTransform"),
                Pair.Key,
                Snapshot.TransformRestoreAttempts,
                *GetNameSafe(ReconnectedPC ? ReconnectedPC->PlayerState : nullptr),
                ReconnectedPC ? 1 : 0,
                ReconnectedPC && ReconnectedPC->GetPawn() ? 1 : 0);
            if (ReconnectedPC && ReconnectedPC->GetPawn())
            {
                CorrectPlayerToCurrentServerTransform(ReconnectedPC, TEXT("ReconnectTransformFallback"));
            }
            CompletedTickets.Add(Pair.Key);
        }
    }

    for (int64 Ticket : CompletedTickets)
    {
        DisconnectedPlayerSnapshots.Remove(Ticket);
    }

    for (int64 Ticket : ExpiredTickets)
    {
        ExpireDisconnectedPlayerSnapshot(Ticket, TEXT("ReconnectGraceTimer"));
    }

    StopReconnectGraceTimerIfIdle();
}

void AMainGameMode::ClearDisconnectedPlayerRoundCards(const TCHAR* Context)
{
    int32 SnapshotCount = 0;
    int32 CardCount = 0;

    for (TPair<int64, FDisconnectedPlayerSnapshot>& Pair : DisconnectedPlayerSnapshots)
    {
        TArray<FOwnedCardInfo>& OwnedCards = Pair.Value.PlayerState.OwnedCards;
        Pair.Value.PlayerState.RevealedCard = FOwnedCardInfo();
        if (OwnedCards.IsEmpty())
        {
            continue;
        }

        ++SnapshotCount;
        CardCount += OwnedCards.Num();
        if (CardGameService)
        {
            CardGameService->ClearDetachedOwnedCardRecords(OwnedCards, Context);
        }
        OwnedCards.Reset();
    }

    if (CardCount > 0)
    {
        UE_LOG(LogManagerCard, Display,
            TEXT("[DS] Reconnect DetachedRoundCardsCleared Snapshots=%d Cards=%d Round=%d Context=%s"),
            SnapshotCount,
            CardCount,
            CurrentRound,
            Context ? Context : TEXT("<NULL>"));
    }
}

EGamePhase AMainGameMode::GetClientPhaseForCurrentServerPhase() const
{
    switch (CurrentServerPhase)
    {
    case EDediServerPhase::TransitionToCard:
    case EDediServerPhase::CardGame:
    case EDediServerPhase::Result:
        return EGamePhase::Card;

    default:
        return EGamePhase::TPS;
    }
}

FName AMainGameMode::GetStreamLevelForCurrentServerPhase() const
{
    switch (CurrentServerPhase)
    {
    case EDediServerPhase::BattleRoyale:
    case EDediServerPhase::PreBattleShop:
    case EDediServerPhase::TransitionToBattle:
        return GetPersistentMainWorldLevelName();

    case EDediServerPhase::TransitionToCard:
    case EDediServerPhase::CardGame:
    case EDediServerPhase::Result:
        return TEXT("Card_Game_Stage");

    default:
        return NAME_None;
    }
}

void AMainGameMode::SynchronizePlayerWithCurrentServerPhase(AMainPlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return;
    }

    const EGamePhase ClientPhase = GetClientPhaseForCurrentServerPhase();
    const FName StreamLevel = GetStreamLevelForCurrentServerPhase();

    PlayerController->Client_SynchronizePhase(ClientPhase);
    PlayerController->SetGameplayInputLocked(
        CurrentServerPhase != EDediServerPhase::BattleRoyale
            || !bBattleRoyaleCardsSpawnedThisPhase,
        TEXT("ReconnectPhaseSync"));

    if (!StreamLevel.IsNone())
    {
        PlayerController->Client_SwitchToLevel(NAME_None, StreamLevel);
        RegisterClientStreamLevelRequest(PlayerController, StreamLevel, TEXT("ReconnectPhaseSync"));
    }
    else
    {
        CorrectPlayerToCurrentServerTransform(PlayerController, TEXT("ReconnectNoStreamLevel"));
    }

    if (bBattleRoyaleCardBundleCommitted)
    {
        RequestClientCardBundleExpectation(PlayerController);
    }

    DS_LOG(TEXT("[DS] Reconnect PhaseSync Player=%s ClientPhase=%d StreamLevel=%s ServerPhase=%s Round=%d"),
        *GetNameSafe(PlayerController->PlayerState),
        static_cast<int32>(ClientPhase),
        *StreamLevel.ToString(),
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound);
}

void AMainGameMode::CorrectPlayerToCurrentServerTransform(AMainPlayerController* PlayerController, const TCHAR* Context)
{
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Pawn || !HasAuthority())
    {
        return;
    }

    if (ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            MoveComp->SetBase(nullptr);
            MoveComp->StopMovementImmediately();
        }
    }

    const FVector ServerLocation = Pawn->GetActorLocation();
    const FRotator ServerRotation = Pawn->GetActorRotation().GetNormalized();
    PlayerController->StartServerAuthoritativePositionCorrection(
        ServerLocation,
        ServerRotation,
        Context);

    DS_LOG(TEXT("[DS] Position AuthorityCorrect Player=%s Location=%s Rotation=%s Context=%s"),
        *GetNameSafe(PlayerController->PlayerState),
        *ServerLocation.ToString(),
        *ServerRotation.ToString(),
        Context ? Context : TEXT("<NULL>"));
}

bool AMainGameMode::TeleportPlayerAuthoritatively(
    AMainPlayerController* PlayerController,
    const FTransform& TargetTransform,
    const TCHAR* Context)
{
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Pawn || !HasAuthority())
    {
        return false;
    }

    if (ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            MoveComp->SetBase(nullptr);
            MoveComp->StopMovementImmediately();
        }
    }

    const FVector TargetLocation = TargetTransform.GetLocation();
    const FRotator TargetRotation = TargetTransform.GetRotation().Rotator().GetNormalized();
    if (TargetLocation.ContainsNaN() || TargetRotation.ContainsNaN())
    {
        UE_LOG(LogManager, Error,
            TEXT("[DS] Position AuthorityTeleportFailed Reason=InvalidTarget Player=%s Requested=%s Rotation=%s Context=%s"),
            *GetNameSafe(PlayerController->PlayerState),
            *TargetLocation.ToString(),
            *TargetRotation.ToString(),
            Context ? Context : TEXT("<NULL>"));
        return false;
    }

    const bool bTeleported = Pawn->TeleportTo(TargetLocation, TargetRotation, false, true);
    bool bApplied = bTeleported;
    bool bUsedFallback = false;
    if (!bApplied)
    {
        bUsedFallback = true;
        bApplied = Pawn->SetActorLocationAndRotation(
            TargetLocation,
            TargetRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }

    if (!bApplied)
    {
        UE_LOG(LogManager, Error,
            TEXT("[DS] Position AuthorityTeleportFailed Reason=ApplyFailed Player=%s Requested=%s Rotation=%s Fallback=%d Context=%s"),
            *GetNameSafe(PlayerController->PlayerState),
            *TargetLocation.ToString(),
            *TargetRotation.ToString(),
            bUsedFallback ? 1 : 0,
            Context ? Context : TEXT("<NULL>"));
        return false;
    }

    const FVector AppliedLocation = Pawn->GetActorLocation();
    const FRotator AppliedRotation = Pawn->GetActorRotation().GetNormalized();
    const float LocationError = FVector::Dist(TargetLocation, AppliedLocation);
    const float RotationError = GetRotationErrorDegrees(TargetRotation, AppliedRotation);
    const bool bWithinTolerance =
        LocationError <= PositionTeleportMaxLocationError &&
        RotationError <= PositionTeleportMaxRotationError;

    PlayerController->SetControlRotation(AppliedRotation);
    PlayerController->StartServerAuthoritativePositionCorrection(
        AppliedLocation,
        AppliedRotation,
        Context);

    if (!bWithinTolerance)
    {
        UE_LOG(LogManager, Error,
            TEXT("[DS] Position AuthorityTeleportFailed Reason=OutsideTolerance Player=%s Requested=%s Applied=%s LocationError=%.2f RotationError=%.2f Fallback=%d Context=%s"),
            *GetNameSafe(PlayerController->PlayerState),
            *TargetLocation.ToString(),
            *AppliedLocation.ToString(),
            LocationError,
            RotationError,
            bUsedFallback ? 1 : 0,
            Context ? Context : TEXT("<NULL>"));
        return false;
    }

    DS_LOG(TEXT("[DS] Position AuthorityTeleport Player=%s Requested=%s Applied=%s LocationError=%.2f RotationError=%.2f Teleport=%d Fallback=%d Context=%s"),
        *GetNameSafe(PlayerController->PlayerState),
        *TargetLocation.ToString(),
        *AppliedLocation.ToString(),
        LocationError,
        RotationError,
        bTeleported ? 1 : 0,
        bUsedFallback ? 1 : 0,
        Context ? Context : TEXT("<NULL>"));

    return true;
}

void AMainGameMode::StartReadyPhase()
{
    EndPhase();
    StartTimedServerPhase(EDediServerPhase::Ready, GetReadyDuration());

    if (USpawnManagerComponent* SpawnMgr = USpawnManagerComponent::GetActive(this))
    {
        SpawnMgr->SetShopBarriersActive(true);
    }

    SetPlayerPawnGameplayState(true, false, true, TEXT("Ready"));
}

void AMainGameMode::EnsureBattleRoyaleStageLoaded()
{
    // TransitionToBattle 단계에서 이미 TPS 레벨을 로드했다면 중복 로드를 피한다.
    // (서버 페이즈 머신은 GameMode가 소유하므로, 전략은 이 의미 메서드만 호출한다.)
    const bool bTPSAlreadyLoadedByTransition = CurrentServerPhase == EDediServerPhase::TransitionToBattle;
    if (!bTPSAlreadyLoadedByTransition)
    {
        BroadcastSwitchLevel(NAME_None, GetPersistentMainWorldLevelName());
    }
    else
    {
        DS_LOG(TEXT("[DS] Main SkipDuplicateTPSLoad Round=%d ServerPhase=%s"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
    }
}

void AMainGameMode::HandleClientStreamLevelLoaded(AMainPlayerController* PlayerController, FName LoadedLevel, EGamePhase ClientPhase)
{
    if (!PlayerController)
    {
        return;
    }

    const FName ExpectedLevel = bPendingBattleRoyaleCardSpawn
        ? PendingBattleRoyaleCardSpawnLevel
        : GetStreamLevelForCurrentServerPhase();
    if (ExpectedLevel.IsNone() || LoadedLevel != ExpectedLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] StreamAck ignored. Player=%s Loaded=%s Expected=%s ClientPhase=%d ServerPhase=%s Round=%d"),
            *GetNameSafe(PlayerController->PlayerState),
            *LoadedLevel.ToString(),
            *ExpectedLevel.ToString(),
            static_cast<int32>(ClientPhase),
            GetServerPhaseName(CurrentServerPhase),
            CurrentRound);
        return;
    }

    const TWeakObjectPtr<AMainPlayerController> PlayerKey(PlayerController);
    ClientRequestedStreamLevels.Remove(PlayerKey);
    ClientStreamRequestTimes.Remove(PlayerKey);
    ClientStreamGateExcludedReasons.Remove(PlayerKey);
    ClientLoadedStreamLevels.FindOrAdd(PlayerKey) = LoadedLevel;
    ClientLoadedStreamPhases.FindOrAdd(PlayerKey) = ClientPhase;
    MarkDediRecoveryProgress(TEXT("ClientStreamLevelLoaded"));

    DS_LOG(TEXT("[DS] Main ClientStreamLevelLoaded Player=%s Level=%s ClientPhase=%d Round=%d ServerPhase=%s LoadedClients=%d/%d"),
        *GetNameSafe(PlayerController->PlayerState),
        *LoadedLevel.ToString(),
        static_cast<int32>(ClientPhase),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        ClientLoadedStreamLevels.Num(),
        RequiredPlayerCount);

    CorrectPlayerToCurrentServerTransform(PlayerController, TEXT("ClientStreamLevelLoaded"));
    if (bBattleRoyaleCardBundleCommitted)
    {
        RequestClientCardBundleExpectation(PlayerController);
    }
    TrySpawnBattleRoyaleCardsWhenStreamReady();
}

void AMainGameMode::HandleClientCardBundleReady(
    AMainPlayerController* PlayerController,
    int32 Round,
    int32 BundleGeneration,
    int32 VisibleCount)
{
    if (!PlayerController
        || !bPendingBattleRoyaleCardSpawn
        || !bBattleRoyaleCardBundleCommitted
        || bBattleRoyaleCardsSpawnedThisPhase)
    {
        UE_LOG(LogManagerCard, Warning,
            TEXT("[DS] CardBundleAckIgnored Reason=InactiveGate Player=%s Round=%d Generation=%d Visible=%d Pending=%d Committed=%d Started=%d"),
            *GetNameSafe(PlayerController ? PlayerController->PlayerState : nullptr),
            Round,
            BundleGeneration,
            VisibleCount,
            bPendingBattleRoyaleCardSpawn ? 1 : 0,
            bBattleRoyaleCardBundleCommitted ? 1 : 0,
            bBattleRoyaleCardsSpawnedThisPhase ? 1 : 0);
        return;
    }

    const int32 ExpectedCount = PendingBattleRoyaleCardInstanceIds.Num();
    if (Round != PendingBattleRoyaleCardSpawnRound
        || BundleGeneration != PendingBattleRoyaleCardBundleGeneration
        || VisibleCount != ExpectedCount)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] CardBundleAckRejected Player=%s Round=%d/%d Generation=%d/%d Visible=%d/%d"),
            *GetNameSafe(PlayerController->PlayerState),
            Round,
            PendingBattleRoyaleCardSpawnRound,
            BundleGeneration,
            PendingBattleRoyaleCardBundleGeneration,
            VisibleCount,
            ExpectedCount);
        return;
    }

    const TWeakObjectPtr<AMainPlayerController> PlayerKey(PlayerController);
    ClientReadyCardBundleGenerations.FindOrAdd(PlayerKey) = BundleGeneration;
    MarkDediRecoveryProgress(TEXT("ClientCardBundleReady"));

    int32 ReadyClients = 0;
    int32 TargetClients = 0;
    HaveRequiredClientsReadyForCardBundle(ReadyClients, TargetClients);

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardBundleAckAccepted Player=%s Round=%d Generation=%d Visible=%d ReadyClients=%d/%d"),
        *GetNameSafe(PlayerController->PlayerState),
        Round,
        BundleGeneration,
        VisibleCount,
        ReadyClients,
        TargetClients);

    TrySpawnBattleRoyaleCardsWhenStreamReady();
}

void AMainGameMode::RequestBattleRoyaleCardSpawnAfterStreamReady(const TCHAR* Context)
{
    if (!CardGameService)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card SpawnGate request failed. Reason=NoCardGameService Context=%s"),
            Context ? Context : TEXT("<NULL>"));
        AbortMatchAndShutdown(TEXT("NoCardGameService"));
        return;
    }

    if (bBattleRoyaleCardsSpawnedThisPhase)
    {
        DS_LOG(TEXT("[DS] Card SpawnGate request ignored. Reason=AlreadySpawned Context=%s Round=%d Phase=%s"),
            Context ? Context : TEXT("<NULL>"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    bPendingBattleRoyaleCardSpawn = true;
    BeginDediRecoveryWatchdogStage(TEXT("CardSpawnGate"), Context);
    PendingBattleRoyaleCardSpawnLevel = GetPersistentMainWorldLevelName();
    PendingBattleRoyaleCardSpawnRound = CurrentRound;
    PendingBattleRoyaleCardSpawnContext = Context ? Context : TEXT("<NULL>");
    if (PendingBattleRoyaleCardBundleGeneration <= 0)
    {
        PendingBattleRoyaleCardBundleGeneration = NextBattleRoyaleCardBundleGeneration++;
    }
    BattleRoyaleCardSpawnGateRetryCount = 0;
    LastCardSpawnGateStateMask = MAX_uint8;
    LastCardSpawnGateLoadedClients = INDEX_NONE;
    LastCardSpawnGateTargetClients = INDEX_NONE;
    LastCardSpawnGateExcludedClients = INDEX_NONE;
    LastCardSpawnGateBundleReadyClients = INDEX_NONE;
    LastCardSpawnGateBundleTargetClients = INDEX_NONE;
    LastCardSpawnGateBundleCommitted = INDEX_NONE;

    DS_LOG(TEXT("[DS] Card SpawnGate requested Level=%s Context=%s Round=%d Generation=%d Phase=%s"),
        *PendingBattleRoyaleCardSpawnLevel.ToString(),
        *PendingBattleRoyaleCardSpawnContext,
        CurrentRound,
        PendingBattleRoyaleCardBundleGeneration,
        GetServerPhaseName(CurrentServerPhase));

    TrySpawnBattleRoyaleCardsWhenStreamReady();
}

void AMainGameMode::ResetClientStreamLevelAcks(const TCHAR* Context)
{
    const int32 OldLevelCount = ClientLoadedStreamLevels.Num();
    const int32 OldPhaseCount = ClientLoadedStreamPhases.Num();
    const int32 OldRequestCount = ClientRequestedStreamLevels.Num();
    const int32 OldExcludedCount = ClientStreamGateExcludedReasons.Num();
    ClientLoadedStreamLevels.Empty();
    ClientLoadedStreamPhases.Empty();
    ClientRequestedStreamLevels.Empty();
    ClientStreamRequestTimes.Empty();
    ClientStreamGateExcludedReasons.Empty();

    DS_LOG(TEXT("[DS] Main ClientStreamAckReset Context=%s OldLevels=%d OldPhases=%d OldRequests=%d OldExcluded=%d Round=%d Phase=%s"),
        Context ? Context : TEXT("<NULL>"),
        OldLevelCount,
        OldPhaseCount,
        OldRequestCount,
        OldExcludedCount,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::RegisterClientStreamLevelRequest(AMainPlayerController* PlayerController, FName LevelToLoad, const TCHAR* Context)
{
    if (!PlayerController || LevelToLoad.IsNone())
    {
        return;
    }

    const TWeakObjectPtr<AMainPlayerController> PlayerKey(PlayerController);
    ClientRequestedStreamLevels.FindOrAdd(PlayerKey) = LevelToLoad;
    ClientStreamRequestTimes.FindOrAdd(PlayerKey) = FPlatformTime::Seconds();
    ClientStreamGateExcludedReasons.Remove(PlayerKey);

    DS_LOG(TEXT("[DS] StreamGate request Player=%s Level=%s Context=%s Round=%d Phase=%s"),
        *GetNameSafe(PlayerController->PlayerState),
        *LevelToLoad.ToString(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

bool AMainGameMode::IsStreamGateExcluded(AMainPlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return false;
    }

    return ClientStreamGateExcludedReasons.Contains(TWeakObjectPtr<AMainPlayerController>(PlayerController));
}

int32 AMainGameMode::ExcludeTimedOutStreamGateClients(FName TargetLevel, const TCHAR* Context)
{
    if (TargetLevel.IsNone() || StreamGateClientAckTimeoutSeconds <= 0.0f || !GetWorld())
    {
        return 0;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    int32 NewlyExcluded = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get());
        if (!MainPC || IsStreamGateExcluded(MainPC))
        {
            continue;
        }

        const TWeakObjectPtr<AMainPlayerController> PlayerKey(MainPC);
        const FName* LoadedLevel = ClientLoadedStreamLevels.Find(PlayerKey);
        if (LoadedLevel && *LoadedLevel == TargetLevel)
        {
            continue;
        }

        const FName* RequestedLevel = ClientRequestedStreamLevels.Find(PlayerKey);
        const double* RequestTime = ClientStreamRequestTimes.Find(PlayerKey);
        if (!RequestedLevel || *RequestedLevel != TargetLevel || !RequestTime)
        {
            ClientRequestedStreamLevels.FindOrAdd(PlayerKey) = TargetLevel;
            ClientStreamRequestTimes.FindOrAdd(PlayerKey) = NowSeconds;
            continue;
        }

        const double ElapsedSeconds = NowSeconds - *RequestTime;
        if (ElapsedSeconds < static_cast<double>(StreamGateClientAckTimeoutSeconds))
        {
            continue;
        }

        const FString Reason = FString::Printf(
            TEXT("%s Level=%s Elapsed=%.1fs"),
            Context ? Context : TEXT("<NULL>"),
            *TargetLevel.ToString(),
            ElapsedSeconds);
        ClientStreamGateExcludedReasons.FindOrAdd(PlayerKey) = Reason;
        ++NewlyExcluded;

        UE_LOG(LogTemp, Error, TEXT("[DS] StreamGate timeout. Player=%s Level=%s Elapsed=%.1fs Timeout=%.1fs Kick=%d Context=%s Round=%d Phase=%s"),
            *GetNameSafe(MainPC->PlayerState),
            *TargetLevel.ToString(),
            ElapsedSeconds,
            StreamGateClientAckTimeoutSeconds,
            bKickStreamGateTimedOutClients ? 1 : 0,
            Context ? Context : TEXT("<NULL>"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));

        if (bKickStreamGateTimedOutClients)
        {
            MainPC->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("StreamLevelLoadTimeout")));
        }
    }

    return NewlyExcluded;
}

bool AMainGameMode::HaveRequiredClientsLoadedStreamLevel(FName TargetLevel, int32& OutLoadedClients, int32& OutTargetClients) const
{
    OutLoadedClients = 0;
    OutTargetClients = 0;

    if (TargetLevel.IsNone() || !GetWorld())
    {
        return false;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get());
        if (!MainPC)
        {
            continue;
        }

        if (IsStreamGateExcluded(MainPC))
        {
            continue;
        }

        ++OutTargetClients;

        const TWeakObjectPtr<AMainPlayerController> PlayerKey(MainPC);
        const FName* LoadedLevel = ClientLoadedStreamLevels.Find(PlayerKey);
        if (LoadedLevel && *LoadedLevel == TargetLevel)
        {
            ++OutLoadedClients;
        }
    }

    const int32 RequiredTargets = FMath::Max(1, FMath::Min(RequiredPlayerCount, OutTargetClients));
    return OutTargetClients > 0 && OutLoadedClients >= RequiredTargets;
}

bool AMainGameMode::HaveRequiredClientsReadyForCardBundle(
    int32& OutReadyClients,
    int32& OutTargetClients) const
{
    OutReadyClients = 0;
    OutTargetClients = 0;

    if (!GetWorld()
        || !bBattleRoyaleCardBundleCommitted
        || PendingBattleRoyaleCardBundleGeneration <= 0
        || PendingBattleRoyaleCardInstanceIds.IsEmpty())
    {
        return false;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get());
        if (!MainPC)
        {
            continue;
        }

        ++OutTargetClients;

        const int32* ReadyGeneration = ClientReadyCardBundleGenerations.Find(
            TWeakObjectPtr<AMainPlayerController>(MainPC));
        if (ReadyGeneration && *ReadyGeneration == PendingBattleRoyaleCardBundleGeneration)
        {
            ++OutReadyClients;
        }
    }

    return OutTargetClients >= FMath::Max(1, RequiredPlayerCount)
        && OutReadyClients == OutTargetClients;
}

void AMainGameMode::RequestClientCardBundleExpectation(AMainPlayerController* PlayerController)
{
    if (!PlayerController
        || !bBattleRoyaleCardBundleCommitted
        || PendingBattleRoyaleCardBundleGeneration <= 0
        || PendingBattleRoyaleCardInstanceIds.IsEmpty())
    {
        return;
    }

    PlayerController->Client_ExpectCardBundle(
        PendingBattleRoyaleCardSpawnRound,
        PendingBattleRoyaleCardBundleGeneration,
        PendingBattleRoyaleCardInstanceIds);

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardBundleExpectationSent Player=%s Round=%d Generation=%d Expected=%d"),
        *GetNameSafe(PlayerController->PlayerState),
        PendingBattleRoyaleCardSpawnRound,
        PendingBattleRoyaleCardBundleGeneration,
        PendingBattleRoyaleCardInstanceIds.Num());
}

void AMainGameMode::RequestClientCardBundleExpectations()
{
    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        RequestClientCardBundleExpectation(Cast<AMainPlayerController>(It->Get()));
    }
}

void AMainGameMode::TrySpawnBattleRoyaleCardsWhenStreamReady()
{
    if (!bPendingBattleRoyaleCardSpawn || bBattleRoyaleCardsSpawnedThisPhase)
    {
        return;
    }

    if (bGameEndReached || CurrentRound != PendingBattleRoyaleCardSpawnRound)
    {
        ClearBattleRoyaleCardSpawnGate(TEXT("RoundChangedOrGameEnd"));
        return;
    }

    if (CurrentServerPhase != EDediServerPhase::BattleRoyale)
    {
        ScheduleBattleRoyaleCardSpawnGateRetry(TEXT("WaitingBattleRoyalePhase"));
        return;
    }

    int32 LoadedClients = 0;
    int32 TargetClients = 0;
    const int32 ExcludedClients = ExcludeTimedOutStreamGateClients(
        PendingBattleRoyaleCardSpawnLevel,
        TEXT("BattleRoyaleCardSpawnGate"));
    const bool bConfiguredMinimumClientsLoaded = HaveRequiredClientsLoadedStreamLevel(
        PendingBattleRoyaleCardSpawnLevel,
        LoadedClients,
        TargetClients);
    const int32 ExcludedTotal = ClientStreamGateExcludedReasons.Num();
    if (ExcludedClients > 0 || ExcludedTotal > 0)
    {
        AbortMatchAndShutdown(FString::Printf(
            TEXT("CardStreamGateTimeout:Excluded=%d"),
            ExcludedTotal));
        return;
    }

    const bool bClientsReady = bConfiguredMinimumClientsLoaded
        && ExcludedTotal == 0
        && TargetClients >= FMath::Max(1, RequiredPlayerCount)
        && LoadedClients == TargetClients;

    int32 BundleReadyClients = 0;
    int32 BundleTargetClients = 0;
    const bool bBundleClientsReady = HaveRequiredClientsReadyForCardBundle(
        BundleReadyClients,
        BundleTargetClients);

    UWorld* GateWorld = GetWorld();
    const FServerLevelGateState ServerState = BuildServerLevelGateState(
        GateWorld,
        PendingBattleRoyaleCardSpawnLevel,
        PendingServerStreamLevelToUnload);

    uint8 GateStateMask = 0;
    GateStateMask |= GateWorld ? 1 << 0 : 0;
    GateStateMask |= ServerState.bTargetFound ? 1 << 1 : 0;
    GateStateMask |= ServerState.bTargetLoaded ? 1 << 2 : 0;
    GateStateMask |= ServerState.bTargetVisible ? 1 << 3 : 0;
    GateStateMask |= ServerState.bNavigationSystemPresent ? 1 << 4 : 0;
    GateStateMask |= ServerState.bNavigationBuilding ? 1 << 5 : 0;
    GateStateMask |= bClientsReady ? 1 << 6 : 0;
    GateStateMask |= ServerState.bReady ? 1 << 7 : 0;

    const bool bGateStateChanged =
        GateStateMask != LastCardSpawnGateStateMask ||
        LoadedClients != LastCardSpawnGateLoadedClients ||
        TargetClients != LastCardSpawnGateTargetClients ||
        ExcludedTotal != LastCardSpawnGateExcludedClients ||
        BundleReadyClients != LastCardSpawnGateBundleReadyClients ||
        BundleTargetClients != LastCardSpawnGateBundleTargetClients ||
        static_cast<int32>(bBattleRoyaleCardBundleCommitted) != LastCardSpawnGateBundleCommitted;
    const bool bPeriodicGateSnapshot =
        BattleRoyaleCardSpawnGateRetryCount > 0 &&
        BattleRoyaleCardSpawnGateRetryCount % 120 == 0;

    if (bGateStateChanged || bPeriodicGateSnapshot)
    {
        if (bGateStateChanged)
        {
            MarkDediRecoveryProgress(TEXT("CardGateStateChanged"));
        }

        UE_LOG(LogManagerCard, Display,
            TEXT("[DS] CardGateStatus Level=%s Mode=%s WorldBegunPlay=%d ServerLevelFound=%d ServerLoaded=%d ServerVisible=%d PendingUnload=%s UnloadComplete=%d NavSystem=%d NavBuilding=%d NavDiagnosticOnly=1 ServerReady=%d ClientLoaded=%d/%d ClientsReady=%d ExcludedNow=%d ExcludedTotal=%d BundleCommitted=%d BundleCards=%d BundleReady=%d/%d BundleClientsReady=%d Retry=%d Round=%d Phase=%s"),
            *PendingBattleRoyaleCardSpawnLevel.ToString(),
            ServerState.bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
            ServerState.bWorldBegunPlay ? 1 : 0,
            ServerState.bTargetFound ? 1 : 0,
            ServerState.bTargetLoaded ? 1 : 0,
            ServerState.bTargetVisible ? 1 : 0,
            *PendingServerStreamLevelToUnload.ToString(),
            ServerState.bUnloadComplete ? 1 : 0,
            ServerState.bNavigationSystemPresent ? 1 : 0,
            ServerState.bNavigationBuilding ? 1 : 0,
            ServerState.bReady ? 1 : 0,
            LoadedClients,
            TargetClients,
            bClientsReady ? 1 : 0,
            ExcludedClients,
            ExcludedTotal,
            bBattleRoyaleCardBundleCommitted ? 1 : 0,
            PendingBattleRoyaleCardInstanceIds.Num(),
            BundleReadyClients,
            BundleTargetClients,
            bBundleClientsReady ? 1 : 0,
            BattleRoyaleCardSpawnGateRetryCount,
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));

        LastCardSpawnGateStateMask = GateStateMask;
        LastCardSpawnGateLoadedClients = LoadedClients;
        LastCardSpawnGateTargetClients = TargetClients;
        LastCardSpawnGateExcludedClients = ExcludedTotal;
        LastCardSpawnGateBundleReadyClients = BundleReadyClients;
        LastCardSpawnGateBundleTargetClients = BundleTargetClients;
        LastCardSpawnGateBundleCommitted = bBattleRoyaleCardBundleCommitted ? 1 : 0;
    }

    if (!bClientsReady)
    {
        ScheduleBattleRoyaleCardSpawnGateRetry(TEXT("WaitingClientStreamAck"));
        return;
    }

    if (!ServerState.bReady)
    {
        ScheduleBattleRoyaleCardSpawnGateRetry(TEXT("WaitingServerWorld"));
        return;
    }

    if (!CardGameService)
    {
        UE_LOG(LogManagerCard, Error, TEXT("[DS] Card SpawnGate failed. Reason=NoCardGameService"));
        AbortMatchAndShutdown(TEXT("NoCardGameService"));
        return;
    }

    if (!bBattleRoyaleCardBundleCommitted)
    {
        TArray<int32> SpawnedCardInstanceIds;
        if (!CardGameService->SpawnRoundCardBundleForBattleRoyale(SpawnedCardInstanceIds))
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] CardBundleGateWait Reason=AtomicSpawnFailed Result=NoGameStart Retry=%d Round=%d Generation=%d"),
                BattleRoyaleCardSpawnGateRetryCount,
                CurrentRound,
                PendingBattleRoyaleCardBundleGeneration);
            ScheduleBattleRoyaleCardSpawnGateRetry(TEXT("AtomicCardBundleSpawnFailed"), 2.0f);
            return;
        }

        PendingBattleRoyaleCardInstanceIds = MoveTemp(SpawnedCardInstanceIds);
        ClientReadyCardBundleGenerations.Empty();
        bBattleRoyaleCardBundleCommitted = true;
        MarkDediRecoveryProgress(TEXT("CardBundleCommitted"));

        UE_LOG(LogManagerCard, Display,
            TEXT("[DS] CardBundleCommitted Round=%d Generation=%d Cards=%d Result=WaitingForAllClientVisibilityAcks"),
            PendingBattleRoyaleCardSpawnRound,
            PendingBattleRoyaleCardBundleGeneration,
            PendingBattleRoyaleCardInstanceIds.Num());

        RequestClientCardBundleExpectations();
    }

    BundleReadyClients = 0;
    BundleTargetClients = 0;
    if (!HaveRequiredClientsReadyForCardBundle(BundleReadyClients, BundleTargetClients))
    {
        ScheduleBattleRoyaleCardSpawnGateRetry(TEXT("WaitingClientCardBundleAck"));
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
    }

    bPendingBattleRoyaleCardSpawn = false;
    bBattleRoyaleCardsSpawnedThisPhase = true;

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardGateReady Level=%s Mode=%s StreamClients=%d/%d BundleClients=%d/%d Cards=%d Generation=%d Context=%s Round=%d Phase=%s"),
        *PendingBattleRoyaleCardSpawnLevel.ToString(),
        ServerState.bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
        LoadedClients,
        TargetClients,
        BundleReadyClients,
        BundleTargetClients,
        PendingBattleRoyaleCardInstanceIds.Num(),
        PendingBattleRoyaleCardBundleGeneration,
        *PendingBattleRoyaleCardSpawnContext,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    ClearDediRecoveryWatchdogStage(TEXT("CardGateReady"));
    StartPreBattleShopPhase();
    //StartTimedServerPhase(EDediServerPhase::BattleRoyale, GetBattleRoyaleDuration());
}

void AMainGameMode::ScheduleBattleRoyaleCardSpawnGateRetry(
    const TCHAR* Context,
    float MinimumRetryInterval)
{
    UWorld* World = GetWorld();
    if (!World || !bPendingBattleRoyaleCardSpawn || bBattleRoyaleCardsSpawnedThisPhase)
    {
        return;
    }

    float RetryInterval = FMath::Max(
        FMath::Max(0.05f, BattleRoyaleCardSpawnGateRetryInterval),
        MinimumRetryInterval);
    const bool bStalled =
        BattleRoyaleCardSpawnGateMaxRetries > 0 &&
        BattleRoyaleCardSpawnGateRetryCount >= BattleRoyaleCardSpawnGateMaxRetries;

    if (bStalled)
    {
        RetryInterval = FMath::Max(RetryInterval, BattleRoyaleCardSpawnGateStallRetryInterval);

        if (BattleRoyaleCardSpawnGateRetryCount == BattleRoyaleCardSpawnGateMaxRetries)
        {
            UE_LOG(LogManagerCard, Warning, TEXT("[DS] Card SpawnGate stalled; continue waiting without fallback spawn. Retry=%d Context=%s PendingContext=%s Level=%s Round=%d Phase=%s"),
                BattleRoyaleCardSpawnGateRetryCount,
                Context ? Context : TEXT("<NULL>"),
                *PendingBattleRoyaleCardSpawnContext,
                *PendingBattleRoyaleCardSpawnLevel.ToString(),
                CurrentRound,
                GetServerPhaseName(CurrentServerPhase));
            ++BattleRoyaleCardSpawnGateRetryCount;
        }
    }

    if (World->GetTimerManager().IsTimerActive(BattleRoyaleCardSpawnGateTimerHandle))
    {
        return;
    }

    if (!bStalled)
    {
        ++BattleRoyaleCardSpawnGateRetryCount;
    }
    World->GetTimerManager().SetTimer(
        BattleRoyaleCardSpawnGateTimerHandle,
        this,
        &AMainGameMode::RetryBattleRoyaleCardSpawnGate,
        RetryInterval,
        false);
}

void AMainGameMode::RetryBattleRoyaleCardSpawnGate()
{
    if (UWorld* World = GetWorld())
    {
        // Executing one-shot timers still report active, so release the handle before rescheduling.
        World->GetTimerManager().ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
    }

    TrySpawnBattleRoyaleCardsWhenStreamReady();
}

void AMainGameMode::ClearBattleRoyaleCardSpawnGate(const TCHAR* Context)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
    }

    const bool bRollbackUnstartedBundle = bBattleRoyaleCardBundleCommitted
        && !bBattleRoyaleCardsSpawnedThisPhase;
    if (bRollbackUnstartedBundle && CardGameService)
    {
        CardGameService->ClearCardDrops();
    }

    DS_LOG(TEXT("[DS] Card SpawnGate cleared Context=%s Pending=%d Committed=%d Spawned=%d RolledBack=%d PendingLevel=%s Cards=%d Generation=%d Round=%d Phase=%s"),
        Context ? Context : TEXT("<NULL>"),
        bPendingBattleRoyaleCardSpawn ? 1 : 0,
        bBattleRoyaleCardBundleCommitted ? 1 : 0,
        bBattleRoyaleCardsSpawnedThisPhase ? 1 : 0,
        bRollbackUnstartedBundle ? 1 : 0,
        *PendingBattleRoyaleCardSpawnLevel.ToString(),
        PendingBattleRoyaleCardInstanceIds.Num(),
        PendingBattleRoyaleCardBundleGeneration,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    bPendingBattleRoyaleCardSpawn = false;
    PendingBattleRoyaleCardSpawnLevel = NAME_None;
    PendingBattleRoyaleCardSpawnRound = 0;
    PendingBattleRoyaleCardSpawnContext.Empty();
    PendingBattleRoyaleCardBundleGeneration = 0;
    PendingBattleRoyaleCardInstanceIds.Reset();
    ClientReadyCardBundleGenerations.Empty();
    bBattleRoyaleCardBundleCommitted = false;
    BattleRoyaleCardSpawnGateRetryCount = 0;
    LastCardSpawnGateStateMask = MAX_uint8;
    LastCardSpawnGateLoadedClients = INDEX_NONE;
    LastCardSpawnGateTargetClients = INDEX_NONE;
    LastCardSpawnGateExcludedClients = INDEX_NONE;
    LastCardSpawnGateBundleReadyClients = INDEX_NONE;
    LastCardSpawnGateBundleTargetClients = INDEX_NONE;
    LastCardSpawnGateBundleCommitted = INDEX_NONE;
}

bool AMainGameMode::WaitForTransitionStreamLevelIfNeeded(
    EDediServerPhase TransitionPhase,
    FName TargetLevel)
{
    int32 LoadedClients = 0;
    int32 TargetClients = 0;
    const int32 ExcludedClients = ExcludeTimedOutStreamGateClients(
        TargetLevel,
        TEXT("TransitionStreamGate"));
    const bool bClientsReady = HaveRequiredClientsLoadedStreamLevel(
        TargetLevel,
        LoadedClients,
        TargetClients);
    const FServerLevelGateState ServerState = BuildServerLevelGateState(
        GetWorld(),
        TargetLevel,
        PendingServerStreamLevelToUnload);

    const int32 ExcludedTotal = ClientStreamGateExcludedReasons.Num();
    if (ExcludedClients > 0 || ExcludedTotal > 0)
    {
        AbortMatchAndShutdown(FString::Printf(
            TEXT("TransitionStreamGateTimeout:Phase=%s:Excluded=%d"),
            GetServerPhaseName(TransitionPhase),
            ExcludedTotal));
        return true;
    }

    if (bClientsReady && ServerState.bReady)
    {
        PendingServerStreamLevelToUnload = NAME_None;
        ClearDediRecoveryWatchdogStage(TEXT("TransitionStreamGateReady"));
        return false;
    }

    BeginDediRecoveryWatchdogStage(TEXT("TransitionStreamGate"), GetServerPhaseName(TransitionPhase));
    CurrentServerPhase = TransitionPhase;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PhaseTimerHandle,
            this,
            &AMainGameMode::RetryTransitionStreamGate,
            1.0f,
            false);
    }

    DS_LOG(TEXT("[DS] Transition StreamGate waiting Phase=%s Level=%s Mode=%s WorldBegunPlay=%d ServerFound=%d ServerLoaded=%d ServerVisible=%d PendingUnload=%s UnloadComplete=%d NavSystem=%d NavBuilding=%d ServerReady=%d ClientLoaded=%d/%d ClientsReady=%d ExcludedNow=%d ExcludedTotal=%d Round=%d"),
        GetServerPhaseName(TransitionPhase),
        *TargetLevel.ToString(),
        ServerState.bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
        ServerState.bWorldBegunPlay ? 1 : 0,
        ServerState.bTargetFound ? 1 : 0,
        ServerState.bTargetLoaded ? 1 : 0,
        ServerState.bTargetVisible ? 1 : 0,
        *PendingServerStreamLevelToUnload.ToString(),
        ServerState.bUnloadComplete ? 1 : 0,
        ServerState.bNavigationSystemPresent ? 1 : 0,
        ServerState.bNavigationBuilding ? 1 : 0,
        ServerState.bReady ? 1 : 0,
        LoadedClients,
        TargetClients,
        bClientsReady ? 1 : 0,
        ExcludedClients,
        ClientStreamGateExcludedReasons.Num(),
        CurrentRound);

    return true;
}

void AMainGameMode::RetryTransitionStreamGate()
{
    FinishCurrentServerPhase(TEXT("StreamGateRetry"));
}

void AMainGameMode::StartBattleRoyalePhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartBattleRoyale after GameEnd Round=%d"), CurrentRound);
        return;
    }

    if (USpawnManagerComponent* SpawnMgr = USpawnManagerComponent::GetActive(this))
    {
        SpawnMgr->SetShopBarriersActive(false);
    }

    ClearGoldDrops(TEXT("BattleRoyaleStart"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
    }
    bPendingBattleRoyaleCardSpawn = false;
    bBattleRoyaleCardsSpawnedThisPhase = false;
    bBattleRoyaleCardBundleCommitted = false;
    PendingBattleRoyaleCardSpawnLevel = NAME_None;
    PendingBattleRoyaleCardSpawnRound = CurrentRound;
    PendingBattleRoyaleCardSpawnContext.Empty();
    PendingBattleRoyaleCardInstanceIds.Reset();
    ClientReadyCardBundleGenerations.Empty();
    PendingBattleRoyaleCardBundleGeneration = NextBattleRoyaleCardBundleGeneration++;
    BattleRoyaleCardSpawnGateRetryCount = 0;
    LastCardSpawnGateStateMask = MAX_uint8;
    LastCardSpawnGateLoadedClients = INDEX_NONE;
    LastCardSpawnGateTargetClients = INDEX_NONE;
    LastCardSpawnGateExcludedClients = INDEX_NONE;
    LastCardSpawnGateBundleReadyClients = INDEX_NONE;
    LastCardSpawnGateBundleTargetClients = INDEX_NONE;
    LastCardSpawnGateBundleCommitted = INDEX_NONE;

    // 배틀로얄 진입 셋업(레벨/모드 전환, 카드 스폰 gate, 폰 활성화, 라운드 무기)은
    // UTPSPhaseStrategy::OnPhaseStart 로 이전됨. 제한 시간 타이머는 카드 스폰 성공 후 시작한다.
    BeginPhase(EGamePhase::TPS);

    ClearServerPhaseTimer();
    CurrentServerPhase = EDediServerPhase::BattleRoyale;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    DS_LOG(TEXT("[DS] PhasePrepare Round=%d Phase=%s WaitForCardSpawnGate=1"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    TrySpawnBattleRoyaleCardsWhenStreamReady();
}

void AMainGameMode::StartPreBattleShopPhase()
{
    StartTimedServerPhase(EDediServerPhase::PreBattleShop, GetPreBattleShopDuration());
    if (CurrentServerPhase != EDediServerPhase::PreBattleShop || bGameEndReached)
    {
        return;
    }

    if (AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr)
    {
        GS->SetShopAvailable(true);

        TArray<EUpgradeType> WeaponUpgradePool = {
            EUpgradeType::Weapon_Damage,
            EUpgradeType::Weapon_FireRate,
            EUpgradeType::Weapon_Range,
            EUpgradeType::Weapon_Magazine,
            EUpgradeType::Weapon_Reload
        };
        GS->ShopWeaponUpgradeOptions.Reset();
        for (int32 i = 0; i < 3 && WeaponUpgradePool.Num() > 0; ++i)
        {
            const int32 PickIndex = FMath::RandRange(0, WeaponUpgradePool.Num() - 1);
            GS->ShopWeaponUpgradeOptions.Add(WeaponUpgradePool[PickIndex]);
            WeaponUpgradePool.RemoveAt(PickIndex);
        }
        GS->OnShopWeaponUpgradeOptionsChangedNative.Broadcast();

        GS->ForceNetUpdate();
    }

    if (USpawnManagerComponent* SpawnMgr = USpawnManagerComponent::GetActive(this))
    {
        SpawnMgr->SetShopBarriersActive(true);
    }

    SetPlayerPawnGameplayState(true, true, true, TEXT("PreBattleShop"));
    DS_LOG(TEXT("[DS] PreBattleShop Open Round=%d Duration=%d"),
        CurrentRound,
        RemainingPhaseSeconds);
}

void AMainGameMode::FinishPreBattleShopPhase()
{
    if (AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr)
    {
        GS->SetShopAvailable(false);
        GS->ForceNetUpdate();
    }

    if (USpawnManagerComponent* SpawnMgr = USpawnManagerComponent::GetActive(this))
    {
        SpawnMgr->SetShopBarriersActive(false);
    }

    CloseShopForAllPlayers(TEXT("PreBattleShopFinished"));
    StartTimedServerPhase(EDediServerPhase::BattleRoyale, GetBattleRoyaleDuration());

    if (CurrentServerPhase == EDediServerPhase::BattleRoyale && !bGameEndReached)
    {
        SetPlayerPawnGameplayEnabled(true, TEXT("BattleRoyaleCombatStart"));
    }
}

void AMainGameMode::CloseShopForAllPlayers(const TCHAR* Context)
{
    int32 ControllerCount = 0;
    int32 ClosedShopCount = 0;

    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* PlayerController = Cast<AMainPlayerController>(It->Get());
        if (!PlayerController)
        {
            continue;
        }

        ++ControllerCount;
        if (PlayerController->ForceCloseShopMode(Context))
        {
            ++ClosedShopCount;
        }
    }

    DS_LOG(TEXT("[DS] PreBattleShop Close Controllers=%d OpenShopsClosed=%d Context=%s Round=%d"),
        ControllerCount,
        ClosedShopCount,
        Context ? Context : TEXT("<NULL>"),
        CurrentRound);
}

void AMainGameMode::StartTransitionToCardPhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartTransitionToCard after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // TPS 종료 teardown(폰 비활성/카드 드롭 정리/무브먼트 베이스 정리)은
    // UTPSPhaseStrategy::OnPhaseEnd(EndPhase 호출 시점)로 이전됨. 여기선 전환 글루만 수행.
    ClearBattleRoyaleCardSpawnGate(TEXT("TransitionToCard"));
    EndPhase();
    ClearGoldDrops(TEXT("TransitionToCard"));
    BroadcastSwitchLevel(NAME_None, TEXT("Card_Game_Stage"));
    RequestMovePlayersToCardIslandSeats(TEXT("TransitionToCard"));
    StartTimedServerPhase(EDediServerPhase::TransitionToCard, GetTransitionDuration());
}

void AMainGameMode::StartCardGamePhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartCardGame after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // 카드게임 진입 셋업(좌석 이동, 폰 상태, 3장 보장, 섯다 리셋, 모드 전환)은
    // UCardPhaseStrategy::OnPhaseStart 로 이전됨. GameMode는 페이즈 상태/타이머만 관리한다.
    BeginPhase(EGamePhase::Card);

    ClearServerPhaseTimer();
    CurrentServerPhase = EDediServerPhase::CardGame;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    DS_LOG(TEXT("[DS] PhaseStart Round=%d Phase=%s Duration=0 ManualCardGame=1"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    if (CardGameService)
    {
        CardGameService->StartSeotdaSelectionTimeout();
    }
}

void AMainGameMode::StartResultPhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartResult after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // 라운드 결과 정산(미정산 시 폴백)은 UCardPhaseStrategy::OnPhaseEnd(EndPhase 호출)로 이전됨.
    EndPhase();

    DS_LOG(TEXT("[DS] RoundResult Round=%d Summary=%s"),
        CurrentRound,
        *CardGameService->GetLastRoundResultSummary());

    StartTimedServerPhase(EDediServerPhase::Result, GetResultDuration());
}

void AMainGameMode::StartTransitionToBattlePhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartTransitionToBattle after GameEnd Round=%d"), CurrentRound);
        return;
    }

    CardGameService->ClearCardDrops();
    CardGameService->ClearRoundCardsForAllPlayers();
    ClearDisconnectedPlayerRoundCards(TEXT("TransitionToBattle"));

    RedeployToSpawnPoint();

    SetPlayerPawnGameplayState(true, false, true, TEXT("TransitionToBattle"));
    ClearPlayerPawnMovementBases(TEXT("TransitionToBattle"));
    BroadcastSwitchLevel(TEXT("Card_Game_Stage"), GetPersistentMainWorldLevelName());
    StartTimedServerPhase(EDediServerPhase::TransitionToBattle, GetTransitionDuration());
}
void AMainGameMode::RedeployToSpawnPoint() {
    TArray<AMainPlayerController*> Controllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get()))
        {
            Controllers.Add(MainPC);
        }
    }

    USpawnManagerComponent* SpawnMgr = USpawnManagerComponent::GetActive(this);
    if (!SpawnMgr)
    {
        return;
    }

    // 원본 배열을 제자리에서 섞기만 함(제거 없음) -> 매 라운드 반복 호출해도 풀이 고갈되지 않음.
    SpawnMgr->ShuffleAvailableSpawns();
    const TArray<AA_Spawn*>& Pool = SpawnMgr->GetAvailableSpawnsView();

    int32 SpawnIndex = 0;
    for (AMainPlayerController* aController : Controllers)
    {
        if (SpawnIndex >= Pool.Num())
        {
            UE_LOG(LogTemp, Warning, TEXT("[DS] RedeployToSpawnPoint: spawn pool exhausted Player=%s"),
                *GetNameSafe(aController->PlayerState));
            continue;
        }

        TeleportPlayerAuthoritatively(aController, Pool[SpawnIndex]->GetActorTransform(), TEXT("TransitionToBattleRedeploy"));
        ++SpawnIndex;
    }
}
void AMainGameMode::ClearGoldDrops(const TCHAR* Context)
{
    int32 ClearedCount = 0;
    for (AGoldDropActor* GoldDrop : ActiveGoldDrops)
    {
        if (IsValid(GoldDrop))
        {
            GoldDrop->Destroy();
            ++ClearedCount;
        }
    }
    ActiveGoldDrops.Reset();

    if (ClearedCount > 0)
    {
        UE_LOG(LogManager, Display,
            TEXT("[DS] GoldDrop Cleared Count=%d Context=%s Round=%d"),
            ClearedCount,
            Context ? Context : TEXT("<NULL>"),
            CurrentRound);
    }
}

void AMainGameMode::BuildFinalGoldRanking(
    FString& OutWinnerName,
    FString& OutRankingSummary) const
{
    struct FManagerFinalGoldEntry
    {
        FString PlayerName;
        int32 Gold = 0;
        int64 Ticket = 0;
    };

    TArray<FManagerFinalGoldEntry> Entries;
    auto UpsertEntry = [&Entries](
        int64 Ticket,
        const FString& PlayerName,
        int32 Gold)
    {
        if (Ticket > 0)
        {
            if (FManagerFinalGoldEntry* Existing = Entries.FindByPredicate(
                [Ticket](const FManagerFinalGoldEntry& Entry)
                {
                    return Entry.Ticket == Ticket;
                }))
            {
                Existing->PlayerName = PlayerName;
                Existing->Gold = FMath::Max(0, Gold);
                return;
            }
        }

        FManagerFinalGoldEntry Entry;
        Entry.PlayerName = PlayerName;
        Entry.Gold = FMath::Max(0, Gold);
        Entry.Ticket = Ticket;
        Entries.Add(MoveTemp(Entry));
    };

    for (const TPair<int64, FMatchParticipantGoldRecord>& Pair : MatchParticipantGoldLedger)
    {
        UpsertEntry(Pair.Key, Pair.Value.PlayerName, Pair.Value.Gold);
    }

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            const AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
            const AMainPlayerState* PS = MPC
                ? MPC->GetPlayerState<AMainPlayerState>()
                : nullptr;
            if (!MPC || !PS)
            {
                continue;
            }

            const FString PlayerName = PS->GetPlayerName().IsEmpty()
                ? GetNameSafe(PS)
                : PS->GetPlayerName();
            UpsertEntry(
                GetDediTicketForController(MPC),
                PlayerName,
                PS->CurPlayerData.HoldingGold);
        }
    }

    for (const TPair<int64, FDisconnectedPlayerSnapshot>& Pair : DisconnectedPlayerSnapshots)
    {
        const FString PlayerName = Pair.Value.PlayerName.IsEmpty()
            ? FString::Printf(TEXT("Disconnected_%lld"), Pair.Key)
            : Pair.Value.PlayerName;
        UpsertEntry(
            Pair.Key,
            PlayerName,
            Pair.Value.PlayerState.CurPlayerData.HoldingGold);
    }

    Entries.Sort([](const FManagerFinalGoldEntry& Left, const FManagerFinalGoldEntry& Right)
    {
        if (Left.Gold != Right.Gold)
        {
            return Left.Gold > Right.Gold;
        }

        const int32 NameCompare = Left.PlayerName.Compare(
            Right.PlayerName,
            ESearchCase::IgnoreCase);
        if (NameCompare != 0)
        {
            return NameCompare < 0;
        }
        return Left.Ticket < Right.Ticket;
    });

    if (Entries.IsEmpty())
    {
        OutWinnerName = TEXT("None");
        OutRankingSummary = TEXT("None");
        return;
    }

    TArray<FString> RankingParts;
    int32 CurrentRank = 1;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        if (Index > 0 && Entries[Index].Gold < Entries[Index - 1].Gold)
        {
            CurrentRank = Index + 1;
        }

        RankingParts.Add(FString::Printf(
            TEXT("#%d %s=%d"),
            CurrentRank,
            *Entries[Index].PlayerName,
            Entries[Index].Gold));
    }
    OutRankingSummary = FString::Join(RankingParts, TEXT(" | "));

    TArray<FString> TopPlayerNames;
    const int32 TopGold = Entries[0].Gold;
    for (const FManagerFinalGoldEntry& Entry : Entries)
    {
        if (Entry.Gold != TopGold)
        {
            break;
        }
        TopPlayerNames.Add(Entry.PlayerName);
    }

    OutWinnerName = TopPlayerNames.Num() == 1
        ? TopPlayerNames[0]
        : FString::Printf(
            TEXT("Tie(%s, Money=%d)"),
            *FString::Join(TopPlayerNames, TEXT("/")),
            TopGold);
}

void AMainGameMode::StartGameEndPhase()
{
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] GameEnd ignored duplicate RoomId=%d Round=%d"), DediRoomId, CurrentRound);
        return;
    }

    bGameEndReached = true;
    bGameStarted = false;
    ClearDediRecoveryWatchdogStage(TEXT("GameEnd"));

    EndPhase();

    FString WinnerName = TEXT("None");
    FString MoneySummary = TEXT("None");
    BuildFinalGoldRanking(WinnerName, MoneySummary);

    PendingMatchEndWinnerName = WinnerName;
    PendingMatchEndMoneySummary = MoneySummary;
    bMatchEndFinalizationStarted = false;

    ClearGoldDrops(TEXT("GameEnd"));
    CardGameService->ClearCardDrops();
    CardGameService->ClearRoundCardsForAllPlayers();
    ClearDisconnectedPlayerRoundCards(TEXT("GameEnd"));
    ClearServerPhaseTimer();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReconnectGraceTimerHandle);
    }
    {
        TArray<int64> TicketsToExpire;
        DisconnectedPlayerSnapshots.GetKeys(TicketsToExpire);
        for (int64 Ticket : TicketsToExpire)
        {
            ExpireDisconnectedPlayerSnapshot(Ticket, TEXT("GameEnd"));
        }
    }

    CurrentServerPhase = EDediServerPhase::GameEnd;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    SetPlayerPawnGameplayEnabled(false, TEXT("GameEnd"));

    DS_LOG(TEXT("[DS] GameEnd RoomId=%d Round=%d MaxRound=%d Winner=%s MoneySummary=%s"),
        DediRoomId,
        CurrentRound,
        MaxRoundCount,
        *WinnerName,
        *MoneySummary);

    const FString FinalResultText = FString::Printf(
        TEXT("[MATCH END]\nWinner=%s\nRound=%d/%d\nRanking=%s"),
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

    const float ReturnTimeoutSeconds = FMath::Max(5.0f, MatchEndReturnTimeoutSeconds);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchEndReturnWaitTimerHandle);
        World->GetTimerManager().SetTimer(
            MatchEndReturnWaitTimerHandle,
            this,
            &AMainGameMode::HandleMatchEndReturnTimeout,
            ReturnTimeoutSeconds,
            false);
    }

    DS_LOG(TEXT("[DS] MatchEndReturnWait Started RoomId=%d Round=%d ConnectedPlayers=%d Timeout=%.1f"),
        DediRoomId,
        CurrentRound,
        CountConnectedHumanPlayers(),
        ReturnTimeoutSeconds);

    if (CountConnectedHumanPlayers() <= 0)
    {
        FinalizeMatchEndAndShutdown(TEXT("NoConnectedClients"));
    }
}


void AMainGameMode::HandleMatchEndReturnTimeout()
{
    UE_LOG(LogManager, Warning,
        TEXT("[DS] MatchEndReturnWait Timeout RoomId=%d Round=%d RemainingPlayers=%d Timeout=%.1f Action=Finalize"),
        DediRoomId,
        CurrentRound,
        CountConnectedHumanPlayers(),
        MatchEndReturnTimeoutSeconds);

    FinalizeMatchEndAndShutdown(TEXT("ReturnTimeout"));
}

void AMainGameMode::FinalizeMatchEndAndShutdown(const TCHAR* Reason)
{
    if (!bGameEndReached || bMatchAbortRequested || bMatchEndFinalizationStarted)
    {
        DS_LOG(TEXT("[DS] MatchEndFinalize ignored RoomId=%d GameEnd=%d Abort=%d Started=%d Reason=%s"),
            DediRoomId,
            bGameEndReached ? 1 : 0,
            bMatchAbortRequested ? 1 : 0,
            bMatchEndFinalizationStarted ? 1 : 0,
            Reason ? Reason : TEXT("<NULL>"));
        return;
    }

    bMatchEndFinalizationStarted = true;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchEndReturnWaitTimerHandle);
    }

    DS_LOG(TEXT("[DS] MatchEndFinalize RoomId=%d Round=%d RemainingPlayers=%d Winner=%s Reason=%s"),
        DediRoomId,
        CurrentRound,
        CountConnectedHumanPlayers(),
        *PendingMatchEndWinnerName,
        Reason ? Reason : TEXT("<NULL>"));

    NotifyIocpMatchEnd(PendingMatchEndWinnerName, PendingMatchEndMoneySummary);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            MatchEndShutdownTimerHandle,
            this,
            &AMainGameMode::ShutdownDedicatedServerAfterMatchEnd,
            1.0f,
            false);
    }
    else
    {
        FPlatformMisc::RequestExit(false);
    }
}


FCardPlacementService AMainGameMode::MakeCardPlacementService() const
{
    FCardPlacementService Service;
    Service.World = GetWorld();

    Service.CardBundleDropCenter = CardBundleDropCenter;
    Service.CardBundleDropExtent = CardBundleDropExtent;
    Service.CardBundleDropJitterRatio = CardBundleDropJitterRatio;

    Service.CardIslandDropZoneTag = CardIslandDropZoneTag;
    Service.bAutoDetectSeasonIslandActorsAsDropZones = bAutoDetectSeasonIslandActorsAsDropZones;
    Service.CardIslandDropExpectedZoneCount = CardIslandDropExpectedZoneCount;
    Service.CardIslandDropMaxAttemptsPerCard = CardIslandDropMaxAttemptsPerCard;
    Service.CardIslandGroundTraceHalfHeight = CardIslandGroundTraceHalfHeight;
    Service.CardIslandGroundOffsetZ = CardIslandGroundOffsetZ;
    Service.CardIslandMinCardDistance = CardIslandMinCardDistance;
    Service.bProjectCardDropsToNavigation = bProjectCardDropsToNavigation;
    Service.CardIslandNavProjectExtent = CardIslandNavProjectExtent;
    Service.CardIslandMaxGroundSlopeDegrees = CardIslandMaxGroundSlopeDegrees;
    Service.CardIslandOverlapBoxExtent = CardIslandOverlapBoxExtent;
    Service.CardNoDropZoneTag = CardNoDropZoneTag;
    Service.CardIslandMaxGroundZDelta = CardIslandMaxGroundZDelta;
    Service.CardIslandOverheadClearance = CardIslandOverheadClearance;

    Service.CardDeathDropMaxAttemptsPerCard = CardDeathDropMaxAttemptsPerCard;
    Service.CardDeathDropStartRadius = CardDeathDropStartRadius;
    Service.CardDeathDropRadiusStep = CardDeathDropRadiusStep;
    Service.CardDeathDropMaxRadius = CardDeathDropMaxRadius;
    Service.CardDeathDropMinCardDistance = CardDeathDropMinCardDistance;
    Service.CardDeathDropGroundOffsetZ = CardDeathDropGroundOffsetZ;
    Service.CardDeathDropNavProjectExtent = CardDeathDropNavProjectExtent;
    Service.CardDeathDropMaxNavProjectDistance = CardDeathDropMaxNavProjectDistance;

    return Service;
}

void AMainGameMode::SetPlayerPawnGameplayEnabled(bool bEnabled, const TCHAR* Context)
{
    SetPlayerPawnGameplayState(bEnabled, bEnabled, bEnabled, Context);
}

void AMainGameMode::SetPlayerPawnGameplayState(bool bVisible, bool bMovementEnabled, bool bCollisionEnabled, const TCHAR* Context)
{
    int32 TargetCount = 0;
    int32 PawnCount = 0;
    int32 WeaponCount = 0;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC)
        {
            continue;
        }

        if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(PC))
        {
            MainPC->SetGameplayInputLocked(!bMovementEnabled, Context);
        }

        APawn* Pawn = PC->GetPawn();
        if (!Pawn)
        {
            TargetCount++;
            continue;
        }

        if (ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                MoveComp->SetBase(nullptr);
                MoveComp->StopMovementImmediately();

                if (bMovementEnabled)
                {
                    MoveComp->SetMovementMode(MOVE_Walking);
                }
                else
                {
                    MoveComp->DisableMovement();
                }
            }
        }

        if (AMainCharacter* MainCharacter = Cast<AMainCharacter>(Pawn))
        {
            if (AWeapon* EquippedWeapon = MainCharacter->GetEquippedGun())
            {
                EquippedWeapon->SetActorHiddenInGame(!bVisible);
                EquippedWeapon->SetActorEnableCollision(bCollisionEnabled);
                EquippedWeapon->SetActorTickEnabled(bVisible);
                EquippedWeapon->ForceNetUpdate();
                WeaponCount++;
            }
        }

        Pawn->SetReplicateMovement(true);
        Pawn->SetActorEnableCollision(bCollisionEnabled);
        Pawn->SetActorHiddenInGame(!bVisible);
        Pawn->ForceNetUpdate();
        PawnCount++;
        TargetCount++;
    }

    DS_LOG(TEXT("[DS] Main SetPlayerPawnGameplayState visible=%d movement=%d collision=%d controllers=%d pawns=%d weapons=%d Context=%s Round=%d ServerPhase=%s"),
        bVisible ? 1 : 0,
        bMovementEnabled ? 1 : 0,
        bCollisionEnabled ? 1 : 0,
        TargetCount,
        PawnCount,
        WeaponCount,
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

    DS_LOG(TEXT("[DS] Main ClearPlayerPawnMovementBases targets=%d Context=%s Round=%d ServerPhase=%s"),
        TargetCount,
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

TArray<FTransform> AMainGameMode::BuildCardPlayerSeatTransforms(int32 RequiredCount) const
{
    const FCardPlacementService CardPlacement = MakeCardPlacementService();
    TArray<FTransform> SeatTransforms;
    RequiredCount = FMath::Max(0, RequiredCount);
    TArray<AActor*> SeatActors;
    int32 ComponentTaggedSeatCount = 0;

    UWorld* World = GetWorld();
    if (World && !CardPlayerSeatTag.IsNone())
    {
        UGameplayStatics::GetAllActorsWithTag(World, CardPlayerSeatTag, SeatActors);

        for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
        {
            AActor* CandidateActor = *ActorIt;
            if (!CandidateActor || SeatActors.Contains(CandidateActor))
            {
                continue;
            }

            TInlineComponentArray<UActorComponent*> Components(CandidateActor);
            for (UActorComponent* Component : Components)
            {
                if (Component && Component->ComponentTags.Contains(CardPlayerSeatTag))
                {
                    SeatActors.Add(CandidateActor);
                    ComponentTaggedSeatCount++;
                    break;
                }
            }
        }

        SeatActors.Sort([](const AActor& A, const AActor& B)
        {
            return A.GetName() < B.GetName();
        });

        for (AActor* SeatActor : SeatActors)
        {
            if (!SeatActor)
            {
                continue;
            }

            FVector SeatLocation = SeatActor->GetActorLocation();
            SeatLocation.Z += CardPlayerSeatZOffset;
            SeatTransforms.Add(FTransform(SeatActor->GetActorRotation(), SeatLocation));

            if (SeatTransforms.Num() >= RequiredCount)
            {
                break;
            }
        }
    }

    const int32 TaggedSeatCount = SeatTransforms.Num();
    const float SafeSpacing = FMath::Max(100.0f, CardPlayerSeatSpacing);
    const float Radius = RequiredCount <= 1
        ? 0.0f
        : FMath::Max(SafeSpacing, (SafeSpacing * RequiredCount) / (2.0f * PI));
    auto ProjectPointToSeatNavWithExtent = [World](const FVector& QueryLocation, FVector& OutNavLocation, const FVector& ProjectExtent, float Max2DDistance) -> bool
    {
        if (!World)
        {
            return false;
        }

        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!NavSys)
        {
            return false;
        }

        FNavLocation ProjectedLocation;
        if (!NavSys->ProjectPointToNavigation(QueryLocation, ProjectedLocation, ProjectExtent))
        {
            return false;
        }

        if (Max2DDistance > 0.0f && FVector::Dist2D(QueryLocation, ProjectedLocation.Location) > Max2DDistance)
        {
            return false;
        }

        OutNavLocation = ProjectedLocation.Location;
        return true;
    };

    auto ProjectPointToSeatNav = [this, &ProjectPointToSeatNavWithExtent](const FVector& QueryLocation, FVector& OutNavLocation) -> bool
    {
        return ProjectPointToSeatNavWithExtent(QueryLocation, OutNavLocation, CardIslandNavProjectExtent, 0.0f);
    };

    auto BuildSeatRingFromCenter = [&](const FVector& CenterLocation, const FRotator& CenterRotation, const TCHAR* SourceName)
    {
        SeatTransforms.Reset();

        for (int32 SeatIndex = 0; SeatIndex < RequiredCount; ++SeatIndex)
        {
            const float Angle = RequiredCount <= 1
                ? 0.0f
                : (2.0f * PI * static_cast<float>(SeatIndex)) / static_cast<float>(RequiredCount);

            FVector SeatLocation = CenterLocation;
            SeatLocation.X += FMath::Cos(Angle) * Radius;
            SeatLocation.Y += FMath::Sin(Angle) * Radius;
            SeatLocation.Z += CardPlayerSeatZOffset;

            const FVector LookDirection = (CenterLocation - SeatLocation).GetSafeNormal2D();
            const FRotator SeatRotation = LookDirection.IsNearlyZero()
                ? CenterRotation
                : LookDirection.Rotation();

            SeatTransforms.Add(FTransform(SeatRotation, SeatLocation));
        }

        DS_LOG(TEXT("[DS] Card SeatBuild Required=%d Tagged=%d ComponentTagged=%d GeneratedFromCenter=1 Center=%s Radius=%.1f Source=%s Tag=%s Spacing=%.1f ZOffset=%.1f"),
            RequiredCount,
            TaggedSeatCount,
            ComponentTaggedSeatCount,
            *CenterLocation.ToString(),
            Radius,
            SourceName ? SourceName : TEXT("<NULL>"),
            *CardPlayerSeatTag.ToString(),
            SafeSpacing,
            CardPlayerSeatZOffset);
    };

    if (RequiredCount <= 0 || TaggedSeatCount >= RequiredCount)
    {
        DS_LOG(TEXT("[DS] Card SeatBuild Required=%d Tagged=%d ComponentTagged=%d GeneratedFromCenter=0 Fallback=0 FallbackEnabled=%d Tag=%s Spacing=%.1f ZOffset=%.1f"),
            RequiredCount,
            TaggedSeatCount,
            ComponentTaggedSeatCount,
            bUseCardPlayerFallbackSeats ? 1 : 0,
            *CardPlayerSeatTag.ToString(),
            SafeSpacing,
            CardPlayerSeatZOffset);
        return SeatTransforms;
    }

    if (TaggedSeatCount == 1)
    {
        const FTransform CenterTransform = SeatTransforms[0];
        const FVector CenterLocation = CenterTransform.GetLocation() - FVector(0.0f, 0.0f, CardPlayerSeatZOffset);
        const FRotator CenterRotation = CenterTransform.GetRotation().Rotator();
        BuildSeatRingFromCenter(CenterLocation, CenterRotation, TEXT("TaggedCenter"));
        return SeatTransforms;
    }

    if (TaggedSeatCount <= 0 && RequiredCount > 0)
    {
        FVector FallbackCenter = FVector::ZeroVector;
        FRotator FallbackRotation = FRotator::ZeroRotator;
        const TCHAR* FallbackSource = TEXT("None");
        bool bHasFallbackCenter = false;

        const TArray<FName> CenterMarkerTags =
        {
            FName(TEXT("CardCenter")),
            FName(TEXT("CardGameCenter")),
            FName(TEXT("CardIslandCenter")),
            FName(TEXT("CardPlayerCenter"))
        };

        auto HasAnyCenterMarkerTag = [&CenterMarkerTags](const AActor* Actor) -> bool
        {
            if (!Actor)
            {
                return false;
            }

            for (const FName& CenterTag : CenterMarkerTags)
            {
                if (Actor->ActorHasTag(CenterTag))
                {
                    return true;
                }
            }

            TInlineComponentArray<UActorComponent*> Components(Actor);
            for (UActorComponent* Component : Components)
            {
                if (!Component)
                {
                    continue;
                }

                for (const FName& CenterTag : CenterMarkerTags)
                {
                    if (Component->ComponentTags.Contains(CenterTag))
                    {
                        return true;
                    }
                }
            }

            const FString ActorName = Actor->GetName();
            return ActorName.Contains(TEXT("CardCenter"), ESearchCase::IgnoreCase)
                || ActorName.Contains(TEXT("CardGameCenter"), ESearchCase::IgnoreCase)
                || ActorName.Contains(TEXT("CardIslandCenter"), ESearchCase::IgnoreCase)
                || ActorName.Contains(TEXT("CardPlayerCenter"), ESearchCase::IgnoreCase);
        };

        if (World)
        {
            for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
            {
                AActor* CenterActor = *ActorIt;
                if (!HasAnyCenterMarkerTag(CenterActor))
                {
                    continue;
                }

                FallbackCenter = CenterActor->GetActorLocation();
                FallbackRotation = CenterActor->GetActorRotation();
                bHasFallbackCenter = true;
                FallbackSource = TEXT("CardCenterMarker");

                FVector NavCenter = FallbackCenter;
                if (ProjectPointToSeatNav(FallbackCenter, NavCenter))
                {
                    FallbackCenter = NavCenter;
                    FallbackSource = TEXT("CardCenterMarkerNav");
                }

                break;
            }
        }

        if (!bHasFallbackCenter)
        {
            const TArray<FCardIslandDropZone> IslandDropZones = CardPlacement.FindCardIslandDropZones();
            FVector IslandCenterSum = FVector::ZeroVector;
            int32 IslandCenterCount = 0;

            for (const FCardIslandDropZone& DropZone : IslandDropZones)
            {
                if (DropZone.IslandKey == FName(TEXT("Winter")) ||
                    DropZone.IslandKey == FName(TEXT("Spring")) ||
                    DropZone.IslandKey == FName(TEXT("Summer")) ||
                    DropZone.IslandKey == FName(TEXT("Autumn")))
                {
                    IslandCenterSum += DropZone.Center;
                    IslandCenterCount++;
                }
            }

            if (IslandCenterCount >= 2)
            {
                const FVector IslandCentroid = IslandCenterSum / static_cast<float>(IslandCenterCount);
                FVector NavCenter = IslandCentroid;
                if (ProjectPointToSeatNavWithExtent(IslandCentroid, NavCenter, FVector(900.0f, 900.0f, 3000.0f), 1000.0f))
                {
                    FallbackCenter = NavCenter;
                    FallbackRotation = FRotator::ZeroRotator;
                    bHasFallbackCenter = true;
                    FallbackSource = TEXT("SeasonIslandCentroidNav");
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[DS] Card SeatCenterFallbackReject Source=SeasonIslandCentroid Reason=NavProjectFailed Center=%s IslandCount=%d"),
                        *IslandCentroid.ToString(),
                        IslandCenterCount);
                }
            }
        }

        if (!bHasFallbackCenter && bUseCardPlayerFallbackSeats)
        {
            FallbackCenter = CardPlayerFallbackCenter;
            bHasFallbackCenter = true;
            FallbackSource = TEXT("ConfiguredFallback");

            FVector NavCenter = FallbackCenter;
            if (ProjectPointToSeatNav(FallbackCenter, NavCenter))
            {
                FallbackCenter = NavCenter;
                FallbackSource = TEXT("ConfiguredFallbackNav");
            }
        }

        if (!bHasFallbackCenter)
        {
            UE_LOG(LogTemp, Error, TEXT("[DS] Card SeatBuildFail Required=%d Tagged=0 ComponentTagged=%d Reason=NoCardCenterMarkerAndNoCentroidNav Tag=%s. Add ActorTag CardPlayerSeat or CardCenter to the actual card island."),
                RequiredCount,
                ComponentTaggedSeatCount,
                *CardPlayerSeatTag.ToString());
            return SeatTransforms;
        }

        DS_LOG(TEXT("[DS] Card SeatBuildAutoFallback Required=%d Tagged=0 ComponentTagged=%d Source=%s Center=%s FallbackEnabled=%d Tag=%s"),
            RequiredCount,
            ComponentTaggedSeatCount,
            FallbackSource,
            *FallbackCenter.ToString(),
            bUseCardPlayerFallbackSeats ? 1 : 0,
            *CardPlayerSeatTag.ToString());

        BuildSeatRingFromCenter(FallbackCenter, FallbackRotation, FallbackSource);
        return SeatTransforms;
    }

    if (!bUseCardPlayerFallbackSeats && TaggedSeatCount <= 0 && RequiredCount > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card SeatBuildFail Required=%d Tagged=0 FallbackDisabled=1 Tag=%s. Place CardPlayerSeat tagged actors on the card island."),
            RequiredCount,
            *CardPlayerSeatTag.ToString());
        return SeatTransforms;
    }

    if (!bUseCardPlayerFallbackSeats && TaggedSeatCount < RequiredCount)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card SeatBuildFail Required=%d Tagged=%d FallbackDisabled=1 Tag=%s. Add more CardPlayerSeat actors or leave one center marker only."),
            RequiredCount,
            TaggedSeatCount,
            *CardPlayerSeatTag.ToString());
        SeatTransforms.Reset();
        return SeatTransforms;
    }

    while (SeatTransforms.Num() < RequiredCount)
    {
        const int32 SeatIndex = SeatTransforms.Num();
        const float Angle = RequiredCount <= 1
            ? 0.0f
            : (2.0f * PI * static_cast<float>(SeatIndex)) / static_cast<float>(RequiredCount);

        FVector SeatLocation = CardPlayerFallbackCenter;
        SeatLocation.X += FMath::Cos(Angle) * Radius;
        SeatLocation.Y += FMath::Sin(Angle) * Radius;
        SeatLocation.Z += CardPlayerSeatZOffset;

        const FVector LookDirection = (CardPlayerFallbackCenter - SeatLocation).GetSafeNormal2D();
        const FRotator SeatRotation = LookDirection.IsNearlyZero()
            ? FRotator::ZeroRotator
            : LookDirection.Rotation();

        SeatTransforms.Add(FTransform(SeatRotation, SeatLocation));
    }

    DS_LOG(TEXT("[DS] Card SeatBuild Required=%d Tagged=%d GeneratedFromCenter=0 Fallback=%d FallbackEnabled=%d Tag=%s FallbackCenter=%s Spacing=%.1f ZOffset=%.1f"),
        RequiredCount,
        TaggedSeatCount,
        SeatTransforms.Num() - TaggedSeatCount,
        bUseCardPlayerFallbackSeats ? 1 : 0,
        *CardPlayerSeatTag.ToString(),
        *CardPlayerFallbackCenter.ToString(),
        SafeSpacing,
        CardPlayerSeatZOffset);

    return SeatTransforms;
}

void AMainGameMode::RequestMovePlayersToCardIslandSeats(const TCHAR* Context)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CardSeatMoveRetryTimerHandle);
    }

    PendingCardSeatMoveContext = Context ? Context : TEXT("<NULL>");
    CardSeatMoveRetryCount = 0;

    if (!MovePlayersToCardIslandSeats(*PendingCardSeatMoveContext))
    {
        ScheduleCardSeatMoveRetry(*PendingCardSeatMoveContext);
    }
}

void AMainGameMode::ScheduleCardSeatMoveRetry(const TCHAR* Context)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (CardSeatMoveRetryCount >= FMath::Max(0, CardPlayerSeatMoveMaxRetries))
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card SeatMoveRetryGiveUp Retry=%d MaxRetries=%d Context=%s Round=%d ServerPhase=%s"),
            CardSeatMoveRetryCount,
            CardPlayerSeatMoveMaxRetries,
            Context ? Context : TEXT("<NULL>"),
            CurrentRound,
            GetServerPhaseName(CurrentServerPhase));
        return;
    }

    CardSeatMoveRetryCount++;
    const float RetryInterval = FMath::Max(0.05f, CardPlayerSeatMoveRetryInterval);
    World->GetTimerManager().SetTimer(
        CardSeatMoveRetryTimerHandle,
        this,
        &AMainGameMode::RetryMovePlayersToCardIslandSeats,
        RetryInterval,
        false);

    DS_LOG(TEXT("[DS] Card SeatMoveRetryScheduled Retry=%d MaxRetries=%d Interval=%.2f Context=%s Round=%d ServerPhase=%s"),
        CardSeatMoveRetryCount,
        CardPlayerSeatMoveMaxRetries,
        RetryInterval,
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::RetryMovePlayersToCardIslandSeats()
{
    const FString RetryContext = PendingCardSeatMoveContext.IsEmpty()
        ? FString(TEXT("CardSeatRetry"))
        : PendingCardSeatMoveContext;

    if (!MovePlayersToCardIslandSeats(*RetryContext))
    {
        ScheduleCardSeatMoveRetry(*RetryContext);
    }
}

bool AMainGameMode::MovePlayersToCardIslandSeats(const TCHAR* Context)
{
    TArray<AMainPlayerController*> Controllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get()))
        {
            Controllers.Add(MainPC);
        }
    }

    const TArray<FTransform> SeatTransforms = BuildCardPlayerSeatTransforms(Controllers.Num());
    int32 MovedCount = 0;

    for (int32 Index = 0; Index < Controllers.Num(); ++Index)
    {
        AMainPlayerController* MainPC = Controllers[Index];
        APawn* Pawn = MainPC ? MainPC->GetPawn() : nullptr;
        if (!Pawn || !SeatTransforms.IsValidIndex(Index))
        {
            DS_LOG(TEXT("[DS] Card SeatMoveSkip Index=%d HasPC=%d HasPawn=%d HasSeat=%d Context=%s"),
                Index,
                MainPC ? 1 : 0,
                Pawn ? 1 : 0,
                SeatTransforms.IsValidIndex(Index) ? 1 : 0,
                Context ? Context : TEXT("<NULL>"));
            continue;
        }

        const FTransform& TargetTransform = SeatTransforms[Index];
        const FVector TargetLocation = TargetTransform.GetLocation();
        const FRotator TargetRotation = TargetTransform.GetRotation().Rotator();
        const bool bMoved = TeleportPlayerAuthoritatively(MainPC, TargetTransform, Context);
        if (bMoved)
        {
            ++MovedCount;
        }

        DS_LOG(TEXT("[DS] Card SeatMove Player=%s Index=%d Moved=%d Location=%s Rotation=%s Context=%s"),
            *GetNameSafe(MainPC->PlayerState),
            Index,
            bMoved ? 1 : 0,
            *TargetLocation.ToString(),
            *TargetRotation.ToString(),
            Context ? Context : TEXT("<NULL>"));
    }

    DS_LOG(TEXT("[DS] Card SeatMoveComplete Moved=%d Planned=%d Context=%s Round=%d ServerPhase=%s"),
        MovedCount,
        Controllers.Num(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    return Controllers.Num() <= 0 || MovedCount >= Controllers.Num();
}

void AMainGameMode::StartTimedServerPhase(EDediServerPhase NewPhase, int32 DurationSeconds)
{
    DS_SCREEN(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("StartTimedServerPhase Phase=%s Dur=%d"), GetServerPhaseName(NewPhase), DurationSeconds));
    if (bGameEndReached)
    {
        DS_LOG(TEXT("[DS] PhaseGuard Ignore StartTimedServerPhase phase=%s after GameEnd Round=%d"),
            GetServerPhaseName(NewPhase),
            CurrentRound);
        return;
    }

    ClearServerPhaseTimer();

    CurrentServerPhase = NewPhase;
    RemainingPhaseSeconds = FMath::Max(0, DurationSeconds);
    SetServerRemainingTime(RemainingPhaseSeconds);

    DS_LOG(TEXT("[DS] PhaseStart Round=%d Phase=%s Duration=%d Debug=%d"),
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
        DS_LOG(TEXT("[DS] PhaseGuard ClearTick after GameEnd Round=%d"), CurrentRound);
        return;
    }

    RemainingPhaseSeconds = FMath::Max(0, RemainingPhaseSeconds - 1);
    SetServerRemainingTime(RemainingPhaseSeconds);

    if (RemainingPhaseSeconds <= 5 || RemainingPhaseSeconds % 10 == 0)
    {
        DS_LOG(TEXT("[DS] PhaseTick Round=%d Phase=%s Remaining=%d"),
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
        DS_LOG(TEXT("[DS] PhaseGuard Ignore FinishCurrentServerPhase after GameEnd Round=%d Reason=%s"),
            CurrentRound,
            Reason ? Reason : TEXT("<NULL>"));
        return;
    }

    const EDediServerPhase FinishedPhase = CurrentServerPhase;

    ClearServerPhaseTimer();

    DS_LOG(TEXT("[DS] PhaseEnd Round=%d Phase=%s Reason=%s"),
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
        if (WaitForTransitionStreamLevelIfNeeded(
            EDediServerPhase::TransitionToCard,
            TEXT("Card_Game_Stage")))
        {
            break;
        }
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
            DS_LOG(TEXT("[DS] NextRound Round=%d/%d"), CurrentRound, MaxRoundCount);
            StartTransitionToBattlePhase();
        }
        break;
    case EDediServerPhase::TransitionToBattle:
        if (WaitForTransitionStreamLevelIfNeeded(
            EDediServerPhase::TransitionToBattle,
            GetPersistentMainWorldLevelName()))
        {
            break;
        }
        StartBattleRoyalePhase();
        break;
    case EDediServerPhase::PreBattleShop:
        FinishPreBattleShopPhase();
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
    case EDediServerPhase::PreBattleShop:
        return TEXT("PreBattleShop");
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

    static void ManagerAppendU64BE(TArray<uint8>& Out, uint64 Value)
    {
        for (int32 Shift = 56; Shift >= 0; Shift -= 8)
        {
            Out.Add(static_cast<uint8>((Value >> Shift) & 0xFF));
        }
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

    static uint16 ManagerReadU16BE(const uint8* Data)
    {
        return static_cast<uint16>(
            (static_cast<uint16>(Data[0]) << 8) |
            static_cast<uint16>(Data[1]));
    }

    static uint32 ManagerReadU32BE(const uint8* Data)
    {
        return
            (static_cast<uint32>(Data[0]) << 24) |
            (static_cast<uint32>(Data[1]) << 16) |
            (static_cast<uint32>(Data[2]) << 8) |
            static_cast<uint32>(Data[3]);
    }

    static uint64 ManagerReadU64BE(const uint8* Data)
    {
        uint64 Value = 0;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            Value = (Value << 8) | static_cast<uint64>(Data[Index]);
        }
        return Value;
    }

    static int32 ManagerRemainingTimeoutMs(double DeadlineSeconds)
    {
        const double RemainingSeconds = DeadlineSeconds - FPlatformTime::Seconds();
        return RemainingSeconds > 0.0
            ? FMath::Max(1, FMath::CeilToInt(RemainingSeconds * 1000.0))
            : 0;
    }

    static bool ManagerRecvExactUntilDeadline(
        FSocket* Socket,
        uint8* Buffer,
        int32 BytesToRead,
        double DeadlineSeconds)
    {
        if (!Socket || BytesToRead < 0 || (BytesToRead > 0 && !Buffer))
        {
            return false;
        }

        if (BytesToRead == 0)
        {
            return true;
        }

        int32 TotalBytesRead = 0;
        while (TotalBytesRead < BytesToRead)
        {
            const int32 RemainingMs = ManagerRemainingTimeoutMs(DeadlineSeconds);
            if (RemainingMs <= 0)
            {
                return false;
            }

            if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(RemainingMs)))
            {
                return false;
            }

            int32 BytesRead = 0;
            if (!Socket->Recv(Buffer + TotalBytesRead, BytesToRead - TotalBytesRead, BytesRead) || BytesRead <= 0)
            {
                return false;
            }

            TotalBytesRead += BytesRead;
        }

        return true;
    }

    static bool ManagerConnectUntilDeadline(
        FSocket* Socket,
        const FInternetAddr& Address,
        double DeadlineSeconds)
    {
        if (!Socket || !Socket->SetNonBlocking(true) || !Socket->Connect(Address))
        {
            return false;
        }

        const int32 RemainingMs = ManagerRemainingTimeoutMs(DeadlineSeconds);
        if (RemainingMs <= 0 ||
            !Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromMilliseconds(RemainingMs)))
        {
            return false;
        }

        return Socket->GetConnectionState() == SCS_Connected;
    }

    static bool ManagerSendExactUntilDeadline(
        FSocket* Socket,
        const uint8* Buffer,
        int32 BytesToSend,
        double DeadlineSeconds,
        int32& OutBytesSent)
    {
        OutBytesSent = 0;
        if (!Socket || !Buffer || BytesToSend <= 0)
        {
            return false;
        }

        while (OutBytesSent < BytesToSend)
        {
            const int32 RemainingMs = ManagerRemainingTimeoutMs(DeadlineSeconds);
            if (RemainingMs <= 0 ||
                !Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromMilliseconds(RemainingMs)))
            {
                return false;
            }

            int32 BytesSentThisCall = 0;
            if (!Socket->Send(
                Buffer + OutBytesSent,
                BytesToSend - OutBytesSent,
                BytesSentThisCall))
            {
                return false;
            }

            OutBytesSent += BytesSentThisCall;
        }

        return true;
    }

    static bool SendIocpControlPacketToEndpoint(
        const TCHAR* Context,
        int32 RoomId,
        int32 DediPort,
        uint32 Generation,
        uint64 ControlToken,
        uint16 ExpectedAckType,
        const TArray<uint8>& Packet,
        const FString& Host,
        int32 Port,
        bool bFallbackAttempt,
        int32 AttemptIndex,
        int32 AckTimeoutMs)
    {
        if (Host.IsEmpty() || Port <= 0 || Port > 65535)
        {
            UE_LOG(LogTemp, Error, TEXT("[DS] IOCP %s failed. Invalid endpoint Host=%s Port=%d RoomId=%d Attempt=%d Fallback=%d"),
                Context,
                *Host,
                Port,
                RoomId,
                AttemptIndex,
                bFallbackAttempt ? 1 : 0);
            return false;
        }

        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (!SocketSubsystem)
        {
            UE_LOG(LogTemp, Error, TEXT("[DS] IOCP %s failed. No SocketSubsystem RoomId=%d"), Context, RoomId);
            return false;
        }

        TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
        bool bIpValid = false;
        Addr->SetIp(*Host, bIpValid);
        Addr->SetPort(Port);

        if (!bIpValid)
        {
            UE_LOG(LogTemp, Error, TEXT("[DS] IOCP %s failed. Invalid IP=%s RoomId=%d Attempt=%d Fallback=%d"),
                Context,
                *Host,
                RoomId,
                AttemptIndex,
                bFallbackAttempt ? 1 : 0);
            return false;
        }

        FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, FString(Context), false);
        if (!Socket)
        {
            UE_LOG(LogTemp, Error, TEXT("[DS] IOCP %s failed. CreateSocket RoomId=%d"), Context, RoomId);
            return false;
        }

        const double AttemptDeadlineSeconds =
            FPlatformTime::Seconds() +
            FMath::Max(0.001, static_cast<double>(AckTimeoutMs) / 1000.0);
        const bool bConnected = ManagerConnectUntilDeadline(Socket, *Addr, AttemptDeadlineSeconds);
        int32 TotalBytesSent = 0;
        const bool bSent = bConnected && ManagerSendExactUntilDeadline(
            Socket,
            Packet.GetData(),
            Packet.Num(),
            AttemptDeadlineSeconds,
            TotalBytesSent);

        bool bAckOk = false;
        bool bAckAccepted = false;
        uint16 AckType = 0;
        int32 AckPacketsRead = 0;
        int32 SkippedWelcomePackets = 0;
        uint32 WelcomeSessionId = 0;
        FString AckResult = bSent ? TEXT("NoAck") : TEXT("SendFailed");

        if (bSent)
        {
            constexpr uint16 WelcomePacketType = 10;
            constexpr int32 WelcomePayloadLength = 4;
            constexpr int32 MaxAckPackets = 2;
            constexpr int32 MaxControlPacketSize = 512;

            for (int32 PacketIndex = 0; PacketIndex < MaxAckPackets; ++PacketIndex)
            {
                uint8 AckHeader[4] = {};
                if (!ManagerRecvExactUntilDeadline(
                    Socket,
                    AckHeader,
                    UE_ARRAY_COUNT(AckHeader),
                    AttemptDeadlineSeconds))
                {
                    AckResult = TEXT("AckHeaderTimeout");
                    break;
                }

                const uint16 AckSize = ManagerReadU16BE(AckHeader);
                AckType = ManagerReadU16BE(AckHeader + 2);
                ++AckPacketsRead;

                if (AckSize < 4 || AckSize > MaxControlPacketSize)
                {
                    AckResult = TEXT("BadAckSize");
                    break;
                }

                const int32 AckPayloadLen = static_cast<int32>(AckSize) - 4;
                TArray<uint8> AckPayload;
                AckPayload.SetNumUninitialized(AckPayloadLen);
                if (!ManagerRecvExactUntilDeadline(
                    Socket,
                    AckPayload.GetData(),
                    AckPayloadLen,
                    AttemptDeadlineSeconds))
                {
                    AckResult = TEXT("AckPayloadTimeout");
                    break;
                }

                if (AckType == WelcomePacketType)
                {
                    if (AckPayloadLen != WelcomePayloadLength)
                    {
                        AckResult = TEXT("BadWelcomePayloadLen");
                        break;
                    }
                    if (SkippedWelcomePackets > 0)
                    {
                        AckResult = TEXT("DuplicateWelcome");
                        break;
                    }

                    ++SkippedWelcomePackets;
                    WelcomeSessionId = ManagerReadU32BE(AckPayload.GetData());
                    AckResult = TEXT("WelcomeSkipped");
                    continue;
                }

                if (AckType != ExpectedAckType)
                {
                    AckResult = TEXT("UnexpectedAckType");
                    break;
                }
                if (AckPayloadLen != 19)
                {
                    AckResult = TEXT("BadAckPayloadLen");
                    break;
                }

                const uint32 AckRoomId = ManagerReadU32BE(AckPayload.GetData());
                const uint16 AckDediPort = ManagerReadU16BE(AckPayload.GetData() + 4);
                const uint32 AckGeneration = ManagerReadU32BE(AckPayload.GetData() + 6);
                const uint64 AckControlToken = ManagerReadU64BE(AckPayload.GetData() + 10);
                bAckAccepted = AckPayload[18] != 0;

                const bool bIdentityMatches =
                    AckRoomId == static_cast<uint32>(RoomId) &&
                    AckDediPort == static_cast<uint16>(DediPort) &&
                    AckGeneration == Generation &&
                    AckControlToken == ControlToken;

                if (!bIdentityMatches)
                {
                    AckResult = TEXT("AckIdentityMismatch");
                }
                else if (!bAckAccepted)
                {
                    AckResult = TEXT("AckRejected");
                }
                else
                {
                    bAckOk = true;
                    AckResult = TEXT("OK");
                }
                break;
            }
        }

        DS_LOG(TEXT("[DS] IOCP %s Host=%s Port=%d Attempt=%d Connected=%d Sent=%d Bytes=%d/%d AckPackets=%d AckType=%u AckAccepted=%d WelcomeSkipped=%d WelcomeSession=%u Fallback=%d Result=%s"),
            Context,
            *Host,
            Port,
            AttemptIndex,
            bConnected ? 1 : 0,
            bSent ? 1 : 0,
            TotalBytesSent,
            Packet.Num(),
            AckPacketsRead,
            AckType,
            bAckAccepted ? 1 : 0,
            SkippedWelcomePackets,
            WelcomeSessionId,
            bFallbackAttempt ? 1 : 0,
            *AckResult);

        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
        return bAckOk;
    }

    static bool SendIocpControlPacketWithFallback(
        const TCHAR* Context,
        int32 RoomId,
        int32 DediPort,
        uint32 Generation,
        uint64 ControlToken,
        uint16 ExpectedAckType,
        const TArray<uint8>& Packet,
        const FString& PrimaryHost,
        int32 PrimaryPort,
        int32 MaxAttempts,
        int32 AckTimeoutMs,
        float RetryDelaySeconds)
    {
        const int32 Attempts = FMath::Max(1, MaxAttempts);
        const int32 TimeoutMs = FMath::Max(1, AckTimeoutMs);
        const float RetryDelay = FMath::Max(0.0f, RetryDelaySeconds);

        for (int32 Attempt = 1; Attempt <= Attempts; ++Attempt)
        {
            if (SendIocpControlPacketToEndpoint(
                Context,
                RoomId,
                DediPort,
                Generation,
                ControlToken,
                ExpectedAckType,
                Packet,
                PrimaryHost,
                PrimaryPort,
                false,
                Attempt,
                TimeoutMs))
            {
                return true;
            }

            if (Attempt < Attempts && RetryDelay > 0.0f)
            {
                FPlatformProcess::Sleep(RetryDelay);
            }
        }

        static const FString FallbackHost(TEXT("127.0.0.1"));
        static constexpr int32 FallbackPort = 9000;
        if (PrimaryPort == FallbackPort && PrimaryHost.Equals(FallbackHost, ESearchCase::IgnoreCase))
        {
            return false;
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] IOCP %s retry fallback %s:%d after primary %s:%d failed RoomId=%d Attempts=%d"),
            Context,
            *FallbackHost,
            FallbackPort,
            *PrimaryHost,
            PrimaryPort,
            RoomId,
            Attempts);

        for (int32 Attempt = 1; Attempt <= Attempts; ++Attempt)
        {
            if (SendIocpControlPacketToEndpoint(
                Context,
                RoomId,
                DediPort,
                Generation,
                ControlToken,
                ExpectedAckType,
                Packet,
                FallbackHost,
                FallbackPort,
                true,
                Attempt,
                TimeoutMs))
            {
                return true;
            }

            if (Attempt < Attempts && RetryDelay > 0.0f)
            {
                FPlatformProcess::Sleep(RetryDelay);
            }
        }

        return false;
    }

    static void DispatchIocpControlPacketAsync(
        FString Context,
        int32 RoomId,
        int32 DediPort,
        uint32 Generation,
        uint64 ControlToken,
        uint16 ExpectedAckType,
        TArray<uint8> Packet,
        FString PrimaryHost,
        int32 PrimaryPort,
        int32 MaxAttempts,
        int32 AttemptTimeoutMs,
        float RetryDelaySeconds,
        TFunction<void(bool)> Completion)
    {
        Async(
            EAsyncExecution::ThreadPool,
            [
                Context = MoveTemp(Context),
                RoomId,
                DediPort,
                Generation,
                ControlToken,
                ExpectedAckType,
                Packet = MoveTemp(Packet),
                PrimaryHost = MoveTemp(PrimaryHost),
                PrimaryPort,
                MaxAttempts,
                AttemptTimeoutMs,
                RetryDelaySeconds,
                Completion = MoveTemp(Completion)
            ]() mutable
            {
                const bool bSucceeded = SendIocpControlPacketWithFallback(
                    *Context,
                    RoomId,
                    DediPort,
                    Generation,
                    ControlToken,
                    ExpectedAckType,
                    Packet,
                    PrimaryHost,
                    PrimaryPort,
                    MaxAttempts,
                    AttemptTimeoutMs,
                    RetryDelaySeconds);

                if (bSucceeded)
                {
                    UE_LOG(LogTemp, Display, TEXT("[DS] IOCP %s completed asynchronously RoomId=%d Port=%d Generation=%u"),
                        *Context,
                        RoomId,
                        DediPort,
                        Generation);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[DS] IOCP %s exhausted asynchronously RoomId=%d Port=%d Generation=%u Primary=%s:%d"),
                        *Context,
                        RoomId,
                        DediPort,
                        Generation,
                        *PrimaryHost,
                        PrimaryPort);
                }

                if (Completion)
                {
                    AsyncTask(
                        ENamedThreads::GameThread,
                        [Completion = MoveTemp(Completion), bSucceeded]() mutable
                        {
                            Completion(bSucceeded);
                        });
                }
            });
    }
}

void AMainGameMode::BeginDediRecoveryWatchdogStage(FName Stage, const TCHAR* Context)
{
    if (Stage.IsNone() || bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    if (DediRecoveryWatchdogStage == Stage && DediRecoveryLastProgressTimeSeconds > 0.0)
    {
        return;
    }

    DediRecoveryWatchdogStage = Stage;
    DediRecoveryLastProgressTimeSeconds = FPlatformTime::Seconds();
    DS_LOG(TEXT("[DS] RecoveryWatchdog StageBegin Stage=%s Context=%s Timeout=%.1f RoomId=%d Round=%d Phase=%s"),
        *Stage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        Stage == FName(TEXT("WaitingPlayers"))
            ? DediPlayerJoinNoProgressTimeoutSeconds
            : DediGateNoProgressTimeoutSeconds,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::MarkDediRecoveryProgress(const TCHAR* Context)
{
    if (DediRecoveryWatchdogStage.IsNone() || bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const double PreviousElapsed = DediRecoveryLastProgressTimeSeconds > 0.0
        ? FMath::Max(0.0, NowSeconds - DediRecoveryLastProgressTimeSeconds)
        : 0.0;
    DediRecoveryLastProgressTimeSeconds = NowSeconds;

    DS_LOG(TEXT("[DS] RecoveryWatchdog Progress Stage=%s Context=%s PreviousIdle=%.3f RoomId=%d Round=%d Phase=%s"),
        *DediRecoveryWatchdogStage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        PreviousElapsed,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::ClearDediRecoveryWatchdogStage(const TCHAR* Context)
{
    if (DediRecoveryWatchdogStage.IsNone())
    {
        return;
    }

    const double IdleSeconds = DediRecoveryLastProgressTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - DediRecoveryLastProgressTimeSeconds)
        : 0.0;
    DS_LOG(TEXT("[DS] RecoveryWatchdog StageClear Stage=%s Context=%s Idle=%.3f RoomId=%d Round=%d Phase=%s"),
        *DediRecoveryWatchdogStage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        IdleSeconds,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    DediRecoveryWatchdogStage = NAME_None;
    DediRecoveryLastProgressTimeSeconds = 0.0;
}

void AMainGameMode::TickDediRecoveryWatchdog()
{
    if (bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const int32 HumanPlayers = CountConnectedHumanPlayers();

    if (bGameStarted)
    {
        if (HumanPlayers <= 0)
        {
            if (DediEmptySinceSeconds <= 0.0)
            {
                DediEmptySinceSeconds = NowSeconds;
                UE_LOG(LogTemp, Warning, TEXT("[DS] RecoveryWatchdog empty server detected RoomId=%d Round=%d Phase=%s"),
                    DediRoomId,
                    CurrentRound,
                    GetServerPhaseName(CurrentServerPhase));
            }

            const double EmptyTimeoutSeconds = FMath::Max(
                static_cast<double>(ReconnectGraceSeconds),
                static_cast<double>(DediEmptyServerAbortTimeoutSeconds));
            const double EmptyElapsedSeconds = FMath::Max(0.0, NowSeconds - DediEmptySinceSeconds);
            if (EmptyTimeoutSeconds > 0.0 && EmptyElapsedSeconds >= EmptyTimeoutSeconds)
            {
                AbortMatchAndShutdown(FString::Printf(
                    TEXT("EmptyServerTimeout:Elapsed=%.1f:Phase=%s"),
                    EmptyElapsedSeconds,
                    GetServerPhaseName(CurrentServerPhase)));
                return;
            }
        }
        else if (DediEmptySinceSeconds > 0.0)
        {
            DS_LOG(TEXT("[DS] RecoveryWatchdog empty server recovered RoomId=%d EmptyElapsed=%.3f Players=%d"),
                DediRoomId,
                FMath::Max(0.0, NowSeconds - DediEmptySinceSeconds),
                HumanPlayers);
            DediEmptySinceSeconds = 0.0;
        }
    }
    else
    {
        DediEmptySinceSeconds = 0.0;
    }

    if (DediRecoveryWatchdogStage.IsNone() || DediRecoveryLastProgressTimeSeconds <= 0.0)
    {
        return;
    }

    const double NoProgressTimeoutSeconds =
        DediRecoveryWatchdogStage == FName(TEXT("WaitingPlayers"))
        ? static_cast<double>(DediPlayerJoinNoProgressTimeoutSeconds)
        : static_cast<double>(DediGateNoProgressTimeoutSeconds);
    const double NoProgressElapsedSeconds = FMath::Max(
        0.0,
        NowSeconds - DediRecoveryLastProgressTimeSeconds);
    if (NoProgressTimeoutSeconds > 0.0 &&
        NoProgressElapsedSeconds >= NoProgressTimeoutSeconds)
    {
        AbortMatchAndShutdown(FString::Printf(
            TEXT("NoProgressTimeout:Stage=%s:Elapsed=%.1f:Players=%d"),
            *DediRecoveryWatchdogStage.ToString(),
            NoProgressElapsedSeconds,
            HumanPlayers));
    }
}

void AMainGameMode::AbortMatchAndShutdown(const FString& Reason)
{
    if (bMatchAbortRequested || bGameEndReached)
    {
        DS_LOG(TEXT("[DS] MatchAbort ignored duplicate RoomId=%d Requested=%d GameEnd=%d Reason=%s"),
            DediRoomId,
            bMatchAbortRequested ? 1 : 0,
            bGameEndReached ? 1 : 0,
            *Reason);
        return;
    }

    bMatchAbortRequested = true;
    MatchAbortReason = Reason.IsEmpty() ? TEXT("UnspecifiedMatchAbort") : Reason;
    ClearDediRecoveryWatchdogStage(TEXT("MatchAbort"));
    bGameEndReached = true;
    bGameStarted = false;

    UE_LOG(LogTemp, Error, TEXT("[DS] MatchAbort begin RoomId=%d Port=%d Generation=%u Reason=%s Round=%d Phase=%s Players=%d"),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        *MatchAbortReason,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        CountConnectedHumanPlayers());

    EndPhase();
    ClearServerPhaseTimer();
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(MatchEndShutdownTimerHandle);
        TimerManager.ClearTimer(MatchEndReturnWaitTimerHandle);
        TimerManager.ClearTimer(CardSeatMoveRetryTimerHandle);
        TimerManager.ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
        TimerManager.ClearTimer(ReconnectGraceTimerHandle);
        TimerManager.ClearTimer(DediRecoveryWatchdogTimerHandle);
    }

    if (CardGameService)
    {
        CardGameService->ClearCardDrops();
        CardGameService->ClearRoundCardsForAllPlayers();
    }
    ClearGoldDrops(TEXT("MatchAbort"));
    ClearDisconnectedPlayerRoundCards(TEXT("MatchAbort"));
    SetPlayerPawnGameplayEnabled(false, TEXT("MatchAbort"));
    CurrentServerPhase = EDediServerPhase::GameEnd;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    NotifyIocpMatchAbort(MatchAbortReason);

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get()))
            {
                MainPC->ClientReturnToMainMenuWithTextReason(
                    FText::FromString(FString::Printf(TEXT("MatchAborted: %s"), *MatchAbortReason)));
            }
        }

        GetWorld()->GetTimerManager().SetTimer(
            MatchAbortShutdownTimerHandle,
            this,
            &AMainGameMode::ShutdownDedicatedServerAfterMatchAbort,
            1.0f,
            false);
    }
    else
    {
        ShutdownDedicatedServerAfterMatchAbort();
    }
}

void AMainGameMode::NotifyIocpMatchAbort(const FString& Reason)
{
    constexpr uint16 MatchAbortPacketType = 304;
    constexpr uint16 MatchAbortAckType = 305;

    bMatchAbortControlNotifyComplete = false;
    bMatchAbortControlNotifySucceeded = false;
    bMatchAbortShutdownWaitLogged = false;
    MatchAbortControlNotifyStartTimeSeconds = FPlatformTime::Seconds();

    if (DediRoomId <= 0 || DediPort <= 0 || DediMatchGeneration == 0 || DediControlToken == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchAbortNotify skipped. Invalid control identity RoomId=%d Port=%d Generation=%u TokenPresent=%d Reason=%s"),
            DediRoomId,
            DediPort,
            DediMatchGeneration,
            DediControlToken != 0 ? 1 : 0,
            *Reason);
        bMatchAbortControlNotifyComplete = true;
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendU16BE(Payload, static_cast<uint16>(DediPort));
    ManagerAppendU32BE(Payload, DediMatchGeneration);
    ManagerAppendU64BE(Payload, DediControlToken);
    ManagerAppendUtf8Limited(Payload, Reason, 180);

    TArray<uint8> Packet;
    ManagerAppendU16BE(Packet, static_cast<uint16>(4 + Payload.Num()));
    ManagerAppendU16BE(Packet, MatchAbortPacketType);
    Packet.Append(Payload);

    const TWeakObjectPtr<AMainGameMode> WeakThis(this);
    DispatchIocpControlPacketAsync(
        FString(TEXT("MatchAbortNotify")),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken,
        MatchAbortAckType,
        MoveTemp(Packet),
        DediIocpHost,
        DediIocpPort,
        DediControlNotifyMaxAttempts,
        DediControlAckTimeoutMs,
        DediControlRetryDelaySeconds,
        [WeakThis](bool bSucceeded)
        {
            if (AMainGameMode* GameMode = WeakThis.Get())
            {
                GameMode->HandleIocpMatchAbortNotifyComplete(bSucceeded);
            }
        });
}

void AMainGameMode::HandleIocpMatchAbortNotifyComplete(bool bSucceeded)
{
    bMatchAbortControlNotifyComplete = true;
    bMatchAbortControlNotifySucceeded = bSucceeded;
    DS_LOG(TEXT("[DS] IOCP MatchAbortNotify completion RoomId=%d Success=%d Elapsed=%.3f Reason=%s"),
        DediRoomId,
        bSucceeded ? 1 : 0,
        FMath::Max(0.0, FPlatformTime::Seconds() - MatchAbortControlNotifyStartTimeSeconds),
        *MatchAbortReason);
}

void AMainGameMode::ShutdownDedicatedServerAfterMatchAbort()
{
    constexpr double MatchAbortControlHardDeadlineSeconds = 20.0;
    const double ControlElapsedSeconds = MatchAbortControlNotifyStartTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - MatchAbortControlNotifyStartTimeSeconds)
        : MatchAbortControlHardDeadlineSeconds;

    if (!bMatchAbortControlNotifyComplete &&
        ControlElapsedSeconds < MatchAbortControlHardDeadlineSeconds)
    {
        if (!bMatchAbortShutdownWaitLogged)
        {
            bMatchAbortShutdownWaitLogged = true;
            UE_LOG(LogTemp, Warning, TEXT("[DS] MatchAbort shutdown waiting for IOCP RoomId=%d Elapsed=%.3f HardDeadline=%.1f Reason=%s"),
                DediRoomId,
                ControlElapsedSeconds,
                MatchAbortControlHardDeadlineSeconds,
                *MatchAbortReason);
        }

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                MatchAbortShutdownTimerHandle,
                this,
                &AMainGameMode::ShutdownDedicatedServerAfterMatchAbort,
                0.25f,
                false);
            return;
        }
    }

    if (!bMatchAbortControlNotifyComplete)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] MatchAbort shutdown reached IOCP hard deadline RoomId=%d Elapsed=%.3f Reason=%s"),
            DediRoomId,
            ControlElapsedSeconds,
            *MatchAbortReason);
    }

    UE_LOG(LogTemp, Display, TEXT("[DS] MatchAbort shutdown proceeding RoomId=%d NotifyComplete=%d NotifySucceeded=%d Elapsed=%.3f Reason=%s"),
        DediRoomId,
        bMatchAbortControlNotifyComplete ? 1 : 0,
        bMatchAbortControlNotifySucceeded ? 1 : 0,
        ControlElapsedSeconds,
        *MatchAbortReason);
    FPlatformMisc::RequestExit(false);
}

void AMainGameMode::NotifyIocpServerReady() const
{
    constexpr uint16 ServerReadyPacketType = 301;
    constexpr uint16 ServerReadyAckType = 303;

    if (DediRoomId <= 0 || DediPort <= 0 || DediMatchGeneration == 0 || DediControlToken == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP ServerReadyNotify skipped. Invalid control identity RoomId=%d Port=%d Generation=%u TokenPresent=%d"),
            DediRoomId,
            DediPort,
            DediMatchGeneration,
            DediControlToken != 0 ? 1 : 0);
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendU16BE(Payload, static_cast<uint16>(DediPort));
    ManagerAppendU32BE(Payload, DediMatchGeneration);
    ManagerAppendU64BE(Payload, DediControlToken);

    TArray<uint8> Packet;
    const uint16 TotalSize = static_cast<uint16>(4 + Payload.Num());

    ManagerAppendU16BE(Packet, TotalSize);
    ManagerAppendU16BE(Packet, ServerReadyPacketType);
    Packet.Append(Payload);

    DispatchIocpControlPacketAsync(
        FString(TEXT("ServerReadyNotify")),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken,
        ServerReadyAckType,
        MoveTemp(Packet),
        DediIocpHost,
        DediIocpPort,
        DediControlNotifyMaxAttempts,
        DediControlAckTimeoutMs,
        DediControlRetryDelaySeconds,
        TFunction<void(bool)>());
}

void AMainGameMode::NotifyIocpMatchEnd(const FString& WinnerName, const FString& MoneySummary)
{
    constexpr uint16 MatchEndPacketType = 300;
    constexpr uint16 MatchEndAckType = 302;

    bMatchEndControlNotifyComplete = false;
    bMatchEndControlNotifySucceeded = false;
    bMatchEndShutdownWaitLogged = false;
    MatchEndControlNotifyStartTimeSeconds = FPlatformTime::Seconds();

    if (DediRoomId <= 0 || DediPort <= 0 || DediMatchGeneration == 0 || DediControlToken == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchEndNotify skipped. Invalid control identity RoomId=%d Port=%d Generation=%u TokenPresent=%d Winner=%s"),
            DediRoomId,
            DediPort,
            DediMatchGeneration,
            DediControlToken != 0 ? 1 : 0,
            *WinnerName);
        bMatchEndControlNotifyComplete = true;
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendU16BE(Payload, static_cast<uint16>(DediPort));
    ManagerAppendU32BE(Payload, DediMatchGeneration);
    ManagerAppendU64BE(Payload, DediControlToken);
    ManagerAppendUtf8Limited(Payload, WinnerName, 96);
    ManagerAppendUtf8Limited(Payload, MoneySummary, 180);

    TArray<uint8> Packet;
    const uint16 TotalSize = static_cast<uint16>(4 + Payload.Num());

    ManagerAppendU16BE(Packet, TotalSize);
    ManagerAppendU16BE(Packet, MatchEndPacketType);
    Packet.Append(Payload);

    const TWeakObjectPtr<AMainGameMode> WeakThis(this);
    DispatchIocpControlPacketAsync(
        FString(TEXT("MatchEndNotify")),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken,
        MatchEndAckType,
        MoveTemp(Packet),
        DediIocpHost,
        DediIocpPort,
        DediControlNotifyMaxAttempts,
        DediControlAckTimeoutMs,
        DediControlRetryDelaySeconds,
        [WeakThis](bool bSucceeded)
        {
            if (AMainGameMode* GameMode = WeakThis.Get())
            {
                GameMode->HandleIocpMatchEndNotifyComplete(bSucceeded);
            }
        });
}

void AMainGameMode::HandleIocpMatchEndNotifyComplete(bool bSucceeded)
{
    bMatchEndControlNotifyComplete = true;
    bMatchEndControlNotifySucceeded = bSucceeded;

    DS_LOG(TEXT("[DS] IOCP MatchEndNotify completion delivered to game thread RoomId=%d Success=%d Elapsed=%.3f"),
        DediRoomId,
        bSucceeded ? 1 : 0,
        FMath::Max(0.0, FPlatformTime::Seconds() - MatchEndControlNotifyStartTimeSeconds));
}

void AMainGameMode::ShutdownDedicatedServerAfterMatchEnd()
{
    constexpr double MatchEndControlHardDeadlineSeconds = 20.0;
    const double ControlElapsedSeconds = MatchEndControlNotifyStartTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - MatchEndControlNotifyStartTimeSeconds)
        : MatchEndControlHardDeadlineSeconds;

    if (!bMatchEndControlNotifyComplete && ControlElapsedSeconds < MatchEndControlHardDeadlineSeconds)
    {
        if (!bMatchEndShutdownWaitLogged)
        {
            bMatchEndShutdownWaitLogged = true;
            UE_LOG(LogTemp, Warning, TEXT("[DS] MatchEnd shutdown waiting for IOCP control completion RoomId=%d Elapsed=%.3f HardDeadline=%.1f"),
                DediRoomId,
                ControlElapsedSeconds,
                MatchEndControlHardDeadlineSeconds);
        }

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                MatchEndShutdownTimerHandle,
                this,
                &AMainGameMode::ShutdownDedicatedServerAfterMatchEnd,
                0.25f,
                false);
            return;
        }
    }

    if (!bMatchEndControlNotifyComplete)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] MatchEnd shutdown reached IOCP control hard deadline RoomId=%d Elapsed=%.3f"),
            DediRoomId,
            ControlElapsedSeconds);
    }

    DS_LOG(TEXT("[DS] ShutdownDedicatedServerAfterMatchEnd RoomId=%d Round=%d Phase=%s"),
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    UE_LOG(LogTemp, Display, TEXT("[DS] MatchEnd shutdown proceeding RoomId=%d NotifyComplete=%d NotifySucceeded=%d Elapsed=%.3f"),
        DediRoomId,
        bMatchEndControlNotifyComplete ? 1 : 0,
        bMatchEndControlNotifySucceeded ? 1 : 0,
        ControlElapsedSeconds);

    FPlatformMisc::RequestExit(false);
}

AActor* AMainGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    USpawnManagerComponent* ActiveSpawnManager = USpawnManagerComponent::GetActive(this);
    if (ActiveSpawnManager)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Initialize Random Spawn..."));
        if (ActiveSpawnManager->GetAvailableSpawnCount() == 0)
        {
            ActiveSpawnManager->InitializeSpawnPoints();
        }
        AA_Spawn* RandomSpawn = ActiveSpawnManager->GetUniqueRandomSpawnActor();
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
