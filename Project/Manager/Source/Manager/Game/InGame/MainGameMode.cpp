#include "Game/InGame/MainGameMode.h"
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
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"

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

FString AMainGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options,
    const FString& Portal)
{
    const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    const FString PlayerName = MakePlayerNameFromOptions(Options);

    AMainPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<AMainPlayerState>() : nullptr;
    if (PS && !PlayerName.IsEmpty())
    {
        PS->SetPlayerName(PlayerName);
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main InitNewPlayer PlayerName=%s Controller=%s Options=%s"),
            *PlayerName,
            NewPlayerController ? *NewPlayerController->GetName() : TEXT("<NULL>"),
            *Options);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] Main InitNewPlayer PlayerNameFallback CurrentName=%s Controller=%s Options=%s"),
            PS ? *PS->GetPlayerName() : TEXT("<NO_PLAYER_STATE>"),
            NewPlayerController ? *NewPlayerController->GetName() : TEXT("<NULL>"),
            *Options);
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
    LoadServerStreamLevelForPhase(LevelToLoad, TEXT("BroadcastSwitchLevel"));

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

    FLatentActionInfo LoadInfo;
    LoadInfo.CallbackTarget = this;
    LoadInfo.UUID = ++ServerStreamingLatentActionId;

    UGameplayStatics::LoadStreamLevel(World, LevelToLoad, true, true, LoadInfo);

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main ServerLoadStreamLevel load=%s Context=%s Round=%d ServerPhase=%s UUID=%d"),
        *LevelToLoad.ToString(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        LoadInfo.UUID);
}

bool AMainGameMode::IsBattleRoyalePhase() const
{
    return bGameStarted && CurrentServerPhase == EDediServerPhase::BattleRoyale;
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

void AMainGameMode::EnsureBattleRoyaleStageLoaded()
{
    // TransitionToBattle 단계에서 이미 TPS 레벨을 로드했다면 중복 로드를 피한다.
    // (서버 페이즈 머신은 GameMode가 소유하므로, 전략은 이 의미 메서드만 호출한다.)
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
}

void AMainGameMode::StartBattleRoyalePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartBattleRoyale after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // 배틀로얄 진입 셋업(레벨/모드 전환, 카드 번들 스폰, 폰 활성화, 라운드 무기)은
    // UTPSPhaseStrategy::OnPhaseStart 로 이전됨. GameMode는 타이머 머신만 구동한다.
    BeginPhase(EGamePhase::TPS);
    StartTimedServerPhase(EDediServerPhase::BattleRoyale, GetBattleRoyaleDuration());
}

void AMainGameMode::StartTransitionToCardPhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTransitionToCard after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // TPS 종료 teardown(폰 비활성/카드 드롭 정리/무브먼트 베이스 정리)은
    // UTPSPhaseStrategy::OnPhaseEnd(EndPhase 호출 시점)로 이전됨. 여기선 전환 글루만 수행.
    EndPhase();
    BroadcastSwitchLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));
    RequestMovePlayersToCardIslandSeats(TEXT("TransitionToCard"));
    StartTimedServerPhase(EDediServerPhase::TransitionToCard, GetTransitionDuration());
}

void AMainGameMode::StartCardGamePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartCardGame after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // 카드게임 진입 셋업(좌석 이동, 폰 상태, 3장 보장, 섯다 리셋, 모드 전환)은
    // UCardPhaseStrategy::OnPhaseStart 로 이전됨. GameMode는 페이즈 상태/타이머만 관리한다.
    BeginPhase(EGamePhase::Card);

    ClearServerPhaseTimer();
    CurrentServerPhase = EDediServerPhase::CardGame;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseStart Round=%d Phase=%s Duration=0 ManualCardGame=1"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::StartResultPhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartResult after GameEnd Round=%d"), CurrentRound);
        return;
    }

    // 라운드 결과 정산(미정산 시 폴백)은 UCardPhaseStrategy::OnPhaseEnd(EndPhase 호출)로 이전됨.
    EndPhase();

    UE_LOG(LogTemp, Warning, TEXT("[DS] RoundResult Round=%d Summary=%s"),
        CurrentRound,
        *CardGameService->GetLastRoundResultSummary());

    StartTimedServerPhase(EDediServerPhase::Result, GetResultDuration());
}

void AMainGameMode::StartTransitionToBattlePhase()
{
    if (bGameEndReached)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DS] PhaseGuard Ignore StartTransitionToBattle after GameEnd Round=%d"), CurrentRound);
        return;
    }

    CardGameService->ClearCardDrops();
    CardGameService->ClearRoundCardsForAllPlayers();
    SetPlayerPawnGameplayState(true, false, true, TEXT("TransitionToBattle"));
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
    CardGameService->ClearCardDrops();
    CardGameService->ClearRoundCardsForAllPlayers();
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

            const int32 Money = CardGameService->GetSeotdaPlayerMoney(PS);
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main SetPlayerPawnGameplayState visible=%d movement=%d collision=%d controllers=%d pawns=%d weapons=%d Context=%s Round=%d ServerPhase=%s"),
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Main ClearPlayerPawnMovementBases targets=%d Context=%s Round=%d ServerPhase=%s"),
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

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatBuild Required=%d Tagged=%d ComponentTagged=%d GeneratedFromCenter=1 Center=%s Radius=%.1f Source=%s Tag=%s Spacing=%.1f ZOffset=%.1f"),
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
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatBuild Required=%d Tagged=%d ComponentTagged=%d GeneratedFromCenter=0 Fallback=0 FallbackEnabled=%d Tag=%s Spacing=%.1f ZOffset=%.1f"),
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

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatBuildAutoFallback Required=%d Tagged=0 ComponentTagged=%d Source=%s Center=%s FallbackEnabled=%d Tag=%s"),
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatBuild Required=%d Tagged=%d GeneratedFromCenter=0 Fallback=%d FallbackEnabled=%d Tag=%s FallbackCenter=%s Spacing=%.1f ZOffset=%.1f"),
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatMoveRetryScheduled Retry=%d MaxRetries=%d Interval=%.2f Context=%s Round=%d ServerPhase=%s"),
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
            UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatMoveSkip Index=%d HasPC=%d HasPawn=%d HasSeat=%d Context=%s"),
                Index,
                MainPC ? 1 : 0,
                Pawn ? 1 : 0,
                SeatTransforms.IsValidIndex(Index) ? 1 : 0,
                Context ? Context : TEXT("<NULL>"));
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

        const FVector TargetLocation = SeatTransforms[Index].GetLocation();
        const FRotator TargetRotation = SeatTransforms[Index].GetRotation().Rotator();
        const bool bTeleported = Pawn->TeleportTo(TargetLocation, TargetRotation, false, true);
        if (!bTeleported)
        {
            Pawn->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
        }

        MainPC->SetControlRotation(TargetRotation);
        Pawn->ForceNetUpdate();
        MovedCount++;

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatMove Player=%s Index=%d Teleport=%d Location=%s Rotation=%s Context=%s"),
            *GetNameSafe(MainPC->PlayerState),
            Index,
            bTeleported ? 1 : 0,
            *TargetLocation.ToString(),
            *TargetRotation.ToString(),
            Context ? Context : TEXT("<NULL>"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card SeatMoveComplete Moved=%d Planned=%d Context=%s Round=%d ServerPhase=%s"),
        MovedCount,
        Controllers.Num(),
        Context ? Context : TEXT("<NULL>"),
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    return Controllers.Num() <= 0 || MovedCount >= Controllers.Num();
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
    USpawnManagerComponent* ActiveSpawnManager = USpawnManagerComponent::GetActive(this);
    if (ActiveSpawnManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Initialize Random Spawn..."));
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
