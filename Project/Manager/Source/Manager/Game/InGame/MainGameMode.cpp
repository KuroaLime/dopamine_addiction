#include "Game/InGame/MainGameMode.h"

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
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupReject Reason=CardLimit Player=%s Owned=%d Max=%d"),
            *RequestingPC->GetName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound);
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupOK Player=%s Instance=%d Card=%d OwnedCount=%d"),
        *PS->GetPlayerName(),
        CardInfo.CardInstanceId,
        static_cast<int32>(CardInfo.CardID),
        PS->PublicCardCount);

    return true;
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card PickupNearest Player=%s Instance=%d Distance=%.2f"),
        *RequestingPC->GetName(),
        BestCard->GetCardInstanceId(),
        FMath::Sqrt(BestDistanceSq));

    return TryPickupCard(RequestingPC, BestCard);
}

TArray<ECardID> AMainGameMode::BuildCardBundleIDs() const
{
    TArray<ECardID> CardIDs;

    if (AMainGameState* GS = GetGameState<AMainGameState>())
    {
        GS->CardDataMap.GetKeys(CardIDs);
        CardIDs.Remove(ECardID::None);
    }

    if (CardIDs.Num() == 0)
    {
        for (int32 CardValue = static_cast<int32>(ECardID::Jan_Gwang); CardValue <= static_cast<int32>(ECardID::Oct_Yul); ++CardValue)
        {
            CardIDs.Add(static_cast<ECardID>(CardValue));
        }
    }

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

    const float Aspect = CardBundleDropExtent.Y > 1.0f ? CardBundleDropExtent.X / CardBundleDropExtent.Y : 1.0f;
    const int32 ColumnCount = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TotalCount) * FMath::Max(0.25f, Aspect))));
    const int32 RowCount = FMath::Max(1, FMath::CeilToInt(static_cast<float>(TotalCount) / static_cast<float>(ColumnCount)));

    const int32 Column = Index % ColumnCount;
    const int32 Row = Index / ColumnCount;

    const float FullWidth = CardBundleDropExtent.X * 2.0f;
    const float FullHeight = CardBundleDropExtent.Y * 2.0f;
    const float CellWidth = FullWidth / static_cast<float>(ColumnCount);
    const float CellHeight = FullHeight / static_cast<float>(RowCount);

    const float MinX = CardBundleDropCenter.X - CardBundleDropExtent.X;
    const float MinY = CardBundleDropCenter.Y - CardBundleDropExtent.Y;

    const float JitterRatio = FMath::Clamp(CardBundleDropJitterRatio, 0.0f, 0.45f);
    const float JitterX = CellWidth * JitterRatio;
    const float JitterY = CellHeight * JitterRatio;

    const float X = MinX + (static_cast<float>(Column) + 0.5f) * CellWidth + FMath::FRandRange(-JitterX, JitterX);
    const float Y = MinY + (static_cast<float>(Row) + 0.5f) * CellHeight + FMath::FRandRange(-JitterY, JitterY);
    const float Z = CardBundleDropCenter.Z;

    return FVector(X, Y, Z);
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card Create Instance=%d Card=%d Round=%d"),
        NewInstanceId,
        static_cast<int32>(CardID),
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

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card Drop Instance=%d Card=%d Actor=%s Location=%s Round=%d"),
        InstanceId,
        static_cast<int32>(CardID),
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

    TArray<ECardID> CardIDs = BuildCardBundleIDs();
    ShuffleCardIDs(CardIDs);

    for (int32 Index = 0; Index < CardIDs.Num(); ++Index)
    {
        SpawnCardDrop(CardIDs[Index], GetDistributedCardDropLocation(Index, CardIDs.Num()));
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] Card BundleDropComplete Count=%d Round=%d Phase=%s"),
        ActiveCardDrops.Num(),
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

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card EnsureThree Player=%s Count=%d Max=%d Round=%d"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            TargetCardCount,
            CurrentRound);
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
