// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/Card/CardGameService.h"
#include "Manager.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/MainGameState.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "Game/InGame/Card/CardPlacementService.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "UObject/Package.h"

void UCardGameService::Init(AMainGameMode* InOwner)
{
    OwnerGM = InOwner;
    if (!OwnerGM) return;
    // BP 설정값 복사(런타임 불변).
    CardPickupRange = OwnerGM->GetCardPickupRange();
    MaxCardsPerPlayerPerRound = OwnerGM->GetMaxCardsPerPlayerPerRound();
    CardDropActorClass = OwnerGM->GetCardDropActorClass();
    SeotdaServerSeedPot = OwnerGM->GetSeotdaServerSeedPot();
    SeotdaBaseCallBet = OwnerGM->GetSeotdaBaseCallBet();
}

UWorld* UCardGameService::GetWorld() const
{
    return OwnerGM ? OwnerGM->GetWorld() : nullptr;
}

void UCardGameService::DetachPlayerForReconnect(int64 Ticket, AMainPlayerState* PlayerState)
{
    if (Ticket <= 0 || !PlayerState)
    {
        return;
    }

    const bool bWasBettingActive = bSeotdaBettingActive;

    if (FSeotdaPlayerRoundState* ExistingState = SeotdaRoundStates.Find(PlayerState))
    {
        FSeotdaPlayerRoundState SavedState = *ExistingState;
        SavedState.PlayerState.Reset();
        ReconnectSeotdaStates.Add(Ticket, MoveTemp(SavedState));
        SeotdaRoundStates.Remove(PlayerState);
    }

    TArray<int32> TurnIndices;
    for (int32 Index = 0; Index < SeotdaTurnOrder.Num(); ++Index)
    {
        if (SeotdaTurnOrder[Index].Get() == PlayerState)
        {
            SeotdaTurnOrder[Index].Reset();
            TurnIndices.Add(Index);
        }
    }

    if (TurnIndices.Num() > 0)
    {
        ReconnectTurnOrderIndices.Add(Ticket, MoveTemp(TurnIndices));
    }

    int32 DetachedCardCount = 0;
    for (TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        if (Pair.Value.OwnerPlayerState.Get() == PlayerState)
        {
            Pair.Value.OwnerPlayerState.Reset();
            ++DetachedCardCount;
        }
    }

    DS_LOG(TEXT("[DS] Reconnect CardStateDetached Ticket=%lld Player=%s Cards=%d HasSeotda=%d TurnSlots=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        DetachedCardCount,
        ReconnectSeotdaStates.Contains(Ticket) ? 1 : 0,
        ReconnectTurnOrderIndices.Contains(Ticket) ? ReconnectTurnOrderIndices[Ticket].Num() : 0);

    if (bWasBettingActive && !bSeotdaRoundResolved)
    {
        if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
        {
            ResolveSeotdaRoundResult(TEXT("DisconnectAutoFold"));
            BroadcastSeotdaState();

            if (OwnerGM && OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
            {
                OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaDisconnectSettled"));
            }
        }
        else
        {
            AdvanceSeotdaBettingTurn();
        }
    }
}

void UCardGameService::ReattachPlayerAfterReconnect(int64 Ticket, AMainPlayerState* PlayerState)
{
    if (Ticket <= 0 || !PlayerState)
    {
        return;
    }

    const bool bHasSavedTurnSlots = ReconnectTurnOrderIndices.Contains(Ticket);
    if (FSeotdaPlayerRoundState* SavedState = ReconnectSeotdaStates.Find(Ticket))
    {
        if (bSeotdaBettingActive && !bHasSavedTurnSlots)
        {
            DS_LOG(TEXT("[DS] Reconnect SeotdaStateDrop Ticket=%lld Player=%s Reason=BettingStartedWithoutTurnSlot"),
                Ticket,
                *PlayerState->GetPlayerName());
        }
        else
        {
            SavedState->PlayerState = PlayerState;
            SeotdaRoundStates.Add(PlayerState, *SavedState);
        }
        ReconnectSeotdaStates.Remove(Ticket);
    }

    if (TArray<int32>* TurnIndices = ReconnectTurnOrderIndices.Find(Ticket))
    {
        for (int32 Index : *TurnIndices)
        {
            if (SeotdaTurnOrder.IsValidIndex(Index))
            {
                SeotdaTurnOrder[Index] = PlayerState;
            }
        }
        ReconnectTurnOrderIndices.Remove(Ticket);
    }

    int32 ReattachedCardCount = 0;
    for (const FOwnedCardInfo& CardInfo : PlayerState->OwnedCards)
    {
        if (FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId))
        {
            Record->OwnerPlayerState = PlayerState;
            ++ReattachedCardCount;
        }
    }

    DS_LOG(TEXT("[DS] Reconnect CardStateReattached Ticket=%lld Player=%s Cards=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        ReattachedCardCount);
}

void UCardGameService::ExpireReconnectState(int64 Ticket, const TCHAR* Reason)
{
    if (Ticket <= 0)
    {
        return;
    }

    const bool bHadSeotdaState = ReconnectSeotdaStates.Remove(Ticket) > 0;
    const bool bHadTurnState = ReconnectTurnOrderIndices.Remove(Ticket) > 0;

    DS_LOG(TEXT("[DS] Reconnect CardStateExpired Ticket=%lld HasSeotda=%d HasTurn=%d Reason=%s"),
        Ticket,
        bHadSeotdaState ? 1 : 0,
        bHadTurnState ? 1 : 0,
        Reason ? Reason : TEXT("<NULL>"));
}

void UCardGameService::HandlePlayerDisconnectedAfterLogout(int64 Ticket, const TCHAR* Reason)
{
    if (!OwnerGM || OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame || bSeotdaRoundResolved)
    {
        return;
    }

    if (bSeotdaBettingActive)
    {
        return;
    }

    DS_LOG(TEXT("[DS] Seotda DisconnectAfterLogout Ticket=%lld Reason=%s"),
        Ticket,
        Reason ? Reason : TEXT("<NULL>"));

    TryResolveSeotdaRoundIfReady();
}

void UCardGameService::ClearReconnectSeotdaStateForRound(const TCHAR* Reason)
{
    const int32 SavedStateCount = ReconnectSeotdaStates.Num();
    const int32 SavedTurnCount = ReconnectTurnOrderIndices.Num();
    if (SavedStateCount <= 0 && SavedTurnCount <= 0)
    {
        return;
    }

    ReconnectSeotdaStates.Empty();
    ReconnectTurnOrderIndices.Empty();

    DS_LOG(TEXT("[DS] Reconnect SeotdaRoundStateCleared States=%d Turns=%d Reason=%s"),
        SavedStateCount,
        SavedTurnCount,
        Reason ? Reason : TEXT("<NULL>"));
}

FString UCardGameService::GetOwnedCardsDebugString(const AMainPlayerState* PS) const
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

bool UCardGameService::TryPickupCard(AMainPlayerController* RequestingPC, ACardDropActor* TargetCard)
{
    if (!OwnerGM->HasAuthority())
    {
        return false;
    }

    if (!RequestingPC || !TargetCard)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=InvalidRequest"));
        return false;
    }

    if (!IsCardPickupAllowed())
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()));
        return false;
    }

    if (TargetCard->IsPickedUp())
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=AlreadyPicked Player=%s Instance=%d"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId());
        return false;
    }

    APawn* Pawn = RequestingPC->GetPawn();
    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!Pawn || !PS)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=MissingPawnOrPS Player=%s"), *RequestingPC->GetName());
        return false;
    }

    if (PS->OwnedCards.Num() >= MaxCardsPerPlayerPerRound)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=CardLimit Player=%s Owned=%d Max=%d OwnedCards=[%s]"),
            *RequestingPC->GetName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound,
            *GetOwnedCardsDebugString(PS));

        return false;
    }

    const float Distance = FVector::Dist(Pawn->GetActorLocation(), TargetCard->GetActorLocation());
    if (Distance > CardPickupRange)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=Distance Player=%s Instance=%d Distance=%.2f Range=%.2f"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId(),
            Distance,
            CardPickupRange);
        return false;
    }

    FServerCardRecord* Record = ServerCardRecords.Find(TargetCard->GetCardInstanceId());
    if (!Record)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=NoRecord Player=%s Instance=%d"),
            *RequestingPC->GetName(),
            TargetCard->GetCardInstanceId());
        return false;
    }

    if (Record->State != ECardRuntimeState::WorldDrop || Record->DropActor.Get() != TargetCard)
    {
        DS_LOG(TEXT("[DS] Card PickupReject Reason=StateMismatch Player=%s Instance=%d State=%d"),
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

    DS_LOG(TEXT("[DS] Card PickupOK Player=%s Instance=%d Card=%d Name=%s OwnedCount=%d OwnedCards=[%s]"),
        *PS->GetPlayerName(),
        CardInfo.CardInstanceId,
        static_cast<int32>(CardInfo.CardID),
        *CardDebug::ToString(CardInfo.CardID),
        PS->PublicCardCount,
        *GetOwnedCardsDebugString(PS));


    return true;
}



bool UCardGameService::RevealSeotdaCard(
    AMainPlayerController* RequestingPC,
    bool bCard0,
    bool bCard1,
    bool bCard2,
    FString& OutFailureReason)
{
    OutFailureReason.Reset();

    if (!OwnerGM || !OwnerGM->HasAuthority())
    {
        OutFailureReason = TEXT("NoServerAuthority");
        return false;
    }

    if (!RequestingPC)
    {
        OutFailureReason = TEXT("InvalidRequest");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=InvalidRequest"));
        return false;
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame)
    {
        OutFailureReason = TEXT("InvalidPhase");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()));
        return false;
    }

    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!PS)
    {
        OutFailureReason = TEXT("MissingPlayerState");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=MissingPS Player=%s"), *RequestingPC->GetName());
        return false;
    }

    if (PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
    {
        OutFailureReason = TEXT("InvalidCardCount");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=InvalidCardCount Player=%s Count=%d Required=%d"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound);
        return false;
    }

    const int32 SelectedCount = (bCard0 ? 1 : 0) + (bCard1 ? 1 : 0) + (bCard2 ? 1 : 0);
    if (SelectedCount != 1)
    {
        OutFailureReason = TEXT("InvalidSelectionCount");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=InvalidSelectCount Player=%s Count=%d Required=1"),
            *PS->GetPlayerName(),
            SelectedCount);
        return false;
    }

    FSeotdaPlayerRoundState* ExistingState = SeotdaRoundStates.Find(PS);
    if (ExistingState && ExistingState->bRevealConfirmed)
    {
        OutFailureReason = TEXT("AlreadyRevealed");
        DS_LOG(TEXT("[DS] Seotda RevealReject Reason=AlreadyRevealed Player=%s"), *PS->GetPlayerName());
        return false;
    }

    const bool RevealFlags[3] = { bCard0, bCard1, bCard2 };
    FOwnedCardInfo RevealedCard;

    for (int32 CardIndex = 0; CardIndex < 3; ++CardIndex)
    {
        const FOwnedCardInfo& CardInfo = PS->OwnedCards[CardIndex];
        FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId);
        if (!Record ||
            Record->State != ECardRuntimeState::Owned ||
            Record->OwnerPlayerState.Get() != PS ||
            Record->CardID != CardInfo.CardID)
        {
            OutFailureReason = TEXT("OwnershipRecordMismatch");
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda RevealReject Player=%s Instance=%d Reason=OwnershipRecordMismatch"),
                *PS->GetPlayerName(),
                CardInfo.CardInstanceId);
            return false;
        }

        if (RevealFlags[CardIndex])
        {
            RevealedCard = CardInfo;
        }
    }

    if (RevealedCard.CardInstanceId <= 0)
    {
        OutFailureReason = TEXT("RevealSelectionFailed");
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda RevealReject Player=%s Reason=RevealSelectionFailed Reveal=%d"),
            *PS->GetPlayerName(),
            RevealedCard.CardInstanceId);
        return false;
    }

    FSeotdaPlayerRoundState& State = SeotdaRoundStates.FindOrAdd(PS);
    State.PlayerState = PS;
    State.RevealedCardInstanceId = RevealedCard.CardInstanceId;
    State.bRevealConfirmed = true;
    State.bSubmitted = false;
    State.SelectedCardInstanceIds.Reset();
    State.HandResult = FSeotdaHandResult();

    PS->SetRevealedCard(RevealedCard);

    DS_LOG(TEXT("[DS] Seotda RevealOK Player=%s Public=[#%d:%s] WaitingForFinalHand=1"),
        *PS->GetPlayerName(),
        RevealedCard.CardInstanceId,
        *CardDebug::ToString(RevealedCard.CardID));

    BroadcastSeotdaState();

    return true;
}

bool UCardGameService::SubmitSeotdaSelection(
    AMainPlayerController* RequestingPC,
    bool bCard0,
    bool bCard1,
    bool bCard2,
    FString& OutFailureReason)
{
    OutFailureReason.Reset();

    if (!OwnerGM || !OwnerGM->HasAuthority())
    {
        OutFailureReason = TEXT("NoServerAuthority");
        return false;
    }

    if (!RequestingPC)
    {
        OutFailureReason = TEXT("InvalidRequest");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=InvalidRequest"));
        return false;
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame)
    {
        OutFailureReason = TEXT("InvalidPhase");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()));
        return false;
    }

    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!PS)
    {
        OutFailureReason = TEXT("MissingPlayerState");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=MissingPS Player=%s"), *RequestingPC->GetName());
        return false;
    }

    if (PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
    {
        OutFailureReason = TEXT("InvalidCardCount");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=InvalidCardCount Player=%s Count=%d Required=%d"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            MaxCardsPerPlayerPerRound);
        return false;
    }

    const int32 SelectedCount = (bCard0 ? 1 : 0) + (bCard1 ? 1 : 0) + (bCard2 ? 1 : 0);
    if (SelectedCount != 2)
    {
        OutFailureReason = TEXT("InvalidSelectionCount");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=InvalidSelectCount Player=%s Count=%d Required=2"),
            *PS->GetPlayerName(),
            SelectedCount);
        return false;
    }

    FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
    if (!State || !State->bRevealConfirmed || State->RevealedCardInstanceId <= 0)
    {
        OutFailureReason = TEXT("RevealRequired");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=RevealRequired Player=%s"), *PS->GetPlayerName());
        return false;
    }
    if (State->bSubmitted)
    {
        OutFailureReason = TEXT("AlreadySubmitted");
        DS_LOG(TEXT("[DS] Seotda SubmitReject Reason=AlreadySubmitted Player=%s"), *PS->GetPlayerName());
        return false;
    }

    const FOwnedCardInfo RevealedCard = PS->GetRevealedCard();
    if (RevealedCard.CardInstanceId != State->RevealedCardInstanceId ||
        RevealedCard.CardID == ECardID::None)
    {
        OutFailureReason = TEXT("PublicCardStateMismatch");
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda SubmitReject Player=%s Reason=PublicCardStateMismatch State=%d Replicated=%d"),
            *PS->GetPlayerName(),
            State->RevealedCardInstanceId,
            RevealedCard.CardInstanceId);
        return false;
    }

    const bool SelectionFlags[3] = { bCard0, bCard1, bCard2 };
    TArray<FOwnedCardInfo> HandCards;
    HandCards.Reserve(2);

    for (int32 CardIndex = 0; CardIndex < 3; ++CardIndex)
    {
        const FOwnedCardInfo& CardInfo = PS->OwnedCards[CardIndex];
        FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId);
        if (!Record ||
            Record->State != ECardRuntimeState::Owned ||
            Record->OwnerPlayerState.Get() != PS ||
            Record->CardID != CardInfo.CardID)
        {
            OutFailureReason = TEXT("OwnershipRecordMismatch");
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda SubmitReject Player=%s Instance=%d Reason=OwnershipRecordMismatch"),
                *PS->GetPlayerName(),
                CardInfo.CardInstanceId);
            return false;
        }

        if (SelectionFlags[CardIndex])
        {
            HandCards.Add(CardInfo);
        }
    }

    if (HandCards.Num() != 2)
    {
        OutFailureReason = TEXT("HandSelectionFailed");
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda SubmitReject Player=%s Reason=HandSelectionFailed HandCount=%d"),
            *PS->GetPlayerName(),
            HandCards.Num());
        return false;
    }

    const FSeotdaHandResult HandResult = FSeotdaRuleService::EvaluateSeotdaHand(HandCards[0], HandCards[1]);
    State->HandResult = HandResult;
    State->SelectedCardInstanceIds = HandResult.UsedCardInstanceIds;
    State->bSubmitted = true;

    for (const FOwnedCardInfo& CardInfo : PS->OwnedCards)
    {
        if (FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId))
        {
            if (Record->OwnerPlayerState.Get() == PS)
            {
                Record->State = ECardRuntimeState::Used;
            }
        }
    }

    DS_LOG(TEXT("[DS] Seotda SubmitOK Player=%s Public=[#%d:%s] Hand=[#%d:%s, #%d:%s] Combo=%s Rank=%d SubRank=%d"),
        *PS->GetPlayerName(),
        RevealedCard.CardInstanceId,
        *CardDebug::ToString(RevealedCard.CardID),
        HandCards[0].CardInstanceId,
        *CardDebug::ToString(HandCards[0].CardID),
        HandCards[1].CardInstanceId,
        *CardDebug::ToString(HandCards[1].CardID),
        *HandResult.Name,
        HandResult.Rank,
        HandResult.SubRank);

    BroadcastSeotdaState();

    TryResolveSeotdaRoundIfReady();
    return true;
}

void UCardGameService::ResetSeotdaRoundStates()
{
    ClearSeotdaTimers();
    ClearReconnectSeotdaStateForRound(TEXT("ResetSeotdaRound"));

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AMainPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AMainPlayerState>() : nullptr)
            {
                PS->ClearRevealedCard();
            }
        }
    }

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

    DS_LOG(TEXT("[DS] Seotda Reset Round=%d"), OwnerGM->GetCurrentRound());
    BroadcastSeotdaState();
}

void UCardGameService::StartSeotdaSelectionTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SeotdaSelectionTimeoutTimerHandle);

    const float TimeoutSeconds = OwnerGM->GetSeotdaSelectionTimeoutSeconds();
    if (TimeoutSeconds <= 0.0f)
    {
        DS_LOG(TEXT("[DS] Seotda SelectionTimeout disabled Round=%d"),
            OwnerGM->GetCurrentRound());
        return;
    }

    // Selection is action-driven from here. Prevent the generic CardGame phase
    // timer from advancing the phase before a slow client or this timeout finishes.
    OwnerGM->ClearServerPhaseTimer();
    OwnerGM->SetRemainingPhaseSeconds(0);
    OwnerGM->SetServerRemainingTime(0);

    World->GetTimerManager().SetTimer(
        SeotdaSelectionTimeoutTimerHandle,
        this,
        &UCardGameService::HandleSeotdaSelectionTimeout,
        TimeoutSeconds,
        false);

    DS_LOG(TEXT("[DS] Seotda SelectionTimeout started Round=%d Seconds=%.1f"),
        OwnerGM->GetCurrentRound(),
        TimeoutSeconds);
}

void UCardGameService::HandleSeotdaSelectionTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    TArray<AMainPlayerController*> PendingPlayers;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        const FSeotdaPlayerRoundState* State = PS ? SeotdaRoundStates.Find(PS) : nullptr;
        if (MPC && PS && (!State || !State->bSubmitted))
        {
            PendingPlayers.Add(MPC);
        }
    }

    DS_LOG(TEXT("[DS] Seotda SelectionTimeout fired Round=%d Pending=%d"),
        OwnerGM->GetCurrentRound(),
        PendingPlayers.Num());

    int32 AutoRevealedCount = 0;
    int32 AutoSubmittedCount = 0;

    // First pass: publish every missing public card before any automatic hand submission.
    for (AMainPlayerController* MPC : PendingPlayers)
    {
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        if (!PS || PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoReveal failed Player=%s Reason=InvalidCardCount Count=%d Required=%d"),
                *GetNameSafe(PS),
                PS ? PS->OwnedCards.Num() : 0,
                MaxCardsPerPlayerPerRound);
            continue;
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State || !State->bRevealConfirmed)
        {
            FString RevealFailureReason;
            const bool bRevealAccepted = RevealSeotdaCard(
                MPC,
                true,
                false,
                false,
                RevealFailureReason);

            MPC->Client_ReceiveSeotdaRevealResult(
                bRevealAccepted,
                bRevealAccepted ? FString(TEXT("SelectionTimeoutAutoReveal")) : RevealFailureReason);

            if (!bRevealAccepted)
            {
                continue;
            }

            ++AutoRevealedCount;
            DS_LOG(TEXT("[DS] Seotda AutoReveal Player=%s Instance=%d"),
                *PS->GetPlayerName(),
                PS->OwnedCards[0].CardInstanceId);
        }
    }

    // Second pass: submit a deterministic two-card hand for every player still pending.
    for (AMainPlayerController* MPC : PendingPlayers)
    {
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        if (!PS || PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoSubmit failed Player=%s Reason=InvalidCardCount Count=%d Required=%d"),
                *GetNameSafe(PS),
                PS ? PS->OwnedCards.Num() : 0,
                MaxCardsPerPlayerPerRound);
            continue;
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State || !State->bRevealConfirmed)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoSubmit failed Player=%s Reason=RevealNotConfirmed"),
                *PS->GetPlayerName());
            continue;
        }
        if (State->bSubmitted)
        {
            continue;
        }

        FString SubmitFailureReason;
        const bool bSubmitAccepted = SubmitSeotdaSelection(
            MPC,
            true,
            true,
            false,
            SubmitFailureReason);

        MPC->Client_ReceiveSeotdaSelectionResult(
            bSubmitAccepted,
            bSubmitAccepted ? FString(TEXT("SelectionTimeoutAutoSubmit")) : SubmitFailureReason);

        if (bSubmitAccepted)
        {
            ++AutoSubmittedCount;
            DS_LOG(TEXT("[DS] Seotda AutoSubmit Player=%s HandInstances=%d,%d"),
                *PS->GetPlayerName(),
                PS->OwnedCards[0].CardInstanceId,
                PS->OwnedCards[1].CardInstanceId);
        }
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive ||
        bSeotdaRoundResolved)
    {
        return;
    }

    TryResolveSeotdaRoundIfReady();
    if (!bSeotdaBettingActive && !bSeotdaRoundResolved)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda SelectionTimeout fallback Round=%d Pending=%d AutoRevealed=%d AutoSubmitted=%d Action=ResolveAndAdvance"),
            OwnerGM->GetCurrentRound(),
            PendingPlayers.Num(),
            AutoRevealedCount,
            AutoSubmittedCount);

        ResolveSeotdaRoundResult(TEXT("SelectionTimeoutFallback"));
        BroadcastSeotdaState();
        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaSelectionTimeoutFallback"));
        }
    }
}

void UCardGameService::TryResolveSeotdaRoundIfReady()
{
    if (!OwnerGM->HasAuthority() || !GetWorld())
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

    DS_LOG(TEXT("[DS] Seotda SubmitProgress submitted=%d targets=%d Round=%d"), SubmittedCount, TargetCount, OwnerGM->GetCurrentRound());

    if (TargetCount <= 0 || SubmittedCount < TargetCount || bSeotdaBettingActive)
    {
        return;
    }

    StartSeotdaBettingRound();
    BroadcastSeotdaState();

}

void UCardGameService::StartSeotdaBettingRound()
{
    if (!OwnerGM->HasAuthority() || !GetWorld())
    {
        return;
    }

    SeotdaTurnOrder.Empty();
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SeotdaSelectionTimeoutTimerHandle);
    }

    // 湲곕낯 ?먮룉? ?쒕쾭媛 ?ｋ뒗?? ?뚮젅?댁뼱 ?덉뿉?쒕뒗 鍮좎?吏 ?딅뒗??
    SeotdaPot = FMath::Max(0, SeotdaServerSeedPot);

    // Call???뚮?????媛??뚮젅?댁뼱媛 湲곕낯 2?먯쓣 ?대룄濡??쒖옉 湲곗? 踰좏똿??2濡??붾떎.
    SeotdaCurrentBet = FMath::Max(0, SeotdaBaseCallBet);

    SeotdaCurrentTurnIndex = 0;

    for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        FSeotdaPlayerRoundState& State = Pair.Value;
        if (!State.PlayerState.IsValid() || !State.bSubmitted)
        {
            continue;
        }

        State.bActedThisBetRound = false;
        State.BetMoney = 0;
    }

    bSeotdaBettingActive = true;

    OwnerGM->ClearServerPhaseTimer();
    OwnerGM->SetRemainingPhaseSeconds(0);
    OwnerGM->SetServerRemainingTime(0);

    DS_LOG(TEXT("[DS] Seotda BettingTimerDisabled Round=%d Pot=%d CurrentBet=%d"),
        OwnerGM->GetCurrentRound(),
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
        State->bAllIn = false;
        State->bActedThisBetRound = false;
        State->BetMoney = 0;
        SeotdaTurnOrder.Add(PS);
    }

    DS_LOG(TEXT("[DS] Seotda BettingStart players=%d pot=%d currentBet=%d round=%d"),
        SeotdaTurnOrder.Num(),
        SeotdaPot,
        SeotdaCurrentBet,
        OwnerGM->GetCurrentRound());

    if (SeotdaTurnOrder.Num() <= 1)
    {
        ResolveSeotdaRoundResult(TEXT("SinglePlayer"));
        BroadcastSeotdaState();
        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaSinglePlayerSettled"));
        }
        return;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    DS_LOG(TEXT("[DS] Seotda BetTurn Player=%s Index=%d Pot=%d CurrentBet=%d"),
        TurnPS ? *TurnPS->GetPlayerName() : TEXT("<NULL>"),
        SeotdaCurrentTurnIndex,
        SeotdaPot,
        SeotdaCurrentBet);

    BroadcastSeotdaState();
    StartSeotdaBetTurnTimeout();

}

void UCardGameService::StartSeotdaBetTurnTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        !bSeotdaBettingActive || bSeotdaRoundResolved ||
        !GetCurrentSeotdaTurnPlayer())
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SeotdaBetTurnTimeoutTimerHandle);

    const float TimeoutSeconds = OwnerGM->GetSeotdaBetTurnTimeoutSeconds();
    if (TimeoutSeconds <= 0.0f)
    {
        DS_LOG(TEXT("[DS] Seotda BetTurnTimeout disabled Round=%d"),
            OwnerGM->GetCurrentRound());
        return;
    }

    World->GetTimerManager().SetTimer(
        SeotdaBetTurnTimeoutTimerHandle,
        this,
        &UCardGameService::HandleSeotdaBetTurnTimeout,
        TimeoutSeconds,
        false);

    DS_LOG(TEXT("[DS] Seotda BetTurnTimeout started Round=%d Player=%s Seconds=%.1f"),
        OwnerGM->GetCurrentRound(),
        *GetCurrentSeotdaTurnPlayer()->GetPlayerName(),
        TimeoutSeconds);
}

void UCardGameService::HandleSeotdaBetTurnTimeout()
{
    if (!OwnerGM || !OwnerGM->HasAuthority() || !GetWorld() ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        !bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    FSeotdaPlayerRoundState* State = TurnPS ? SeotdaRoundStates.Find(TurnPS) : nullptr;
    if (!TurnPS || !State || !State->bSubmitted || State->bFolded || State->bAllIn)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda BetTurnTimeout invalid turn Player=%s Action=Advance"),
            *GetNameSafe(TurnPS));
        AdvanceSeotdaBettingTurn();
        return;
    }

    AMainPlayerController* TurnPC = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
        if (MPC && MPC->GetPlayerState<AMainPlayerState>() == TurnPS)
        {
            TurnPC = MPC;
            break;
        }
    }

    const int32 NeedCall = FMath::Max(0, SeotdaCurrentBet - State->BetMoney);
    const EBettingAction AutoAction = NeedCall > 0
        ? EBettingAction::Die
        : EBettingAction::Check;

    DS_LOG(TEXT("[DS] Seotda BetTurnTimeout fired Round=%d Player=%s NeedCall=%d AutoAction=%d"),
        OwnerGM->GetCurrentRound(),
        *TurnPS->GetPlayerName(),
        NeedCall,
        static_cast<int32>(AutoAction));

    if (TurnPC && SubmitSeotdaBetAction(TurnPC, AutoAction))
    {
        return;
    }

    State->bFolded = true;
    State->bActedThisBetRound = true;

    if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
    {
        ResolveSeotdaRoundResult(TEXT("BetTurnTimeoutFallback"));
        BroadcastSeotdaState();
        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaBetTimeoutSettled"));
        }
        return;
    }

    AdvanceSeotdaBettingTurn();
}

void UCardGameService::ClearSeotdaTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SeotdaSelectionTimeoutTimerHandle);
        World->GetTimerManager().ClearTimer(SeotdaBetTurnTimeoutTimerHandle);
    }
}

bool UCardGameService::SubmitSeotdaBetAction(AMainPlayerController* RequestingPC, EBettingAction Action)
{
    if (!OwnerGM->HasAuthority())
    {
        return false;
    }

    if (!RequestingPC)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=InvalidRequest Action=%d"), static_cast<int32>(Action));
        return false;
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=InvalidPhase Player=%s Phase=%s Action=%d"),
            *RequestingPC->GetName(),
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()),
            static_cast<int32>(Action));
        return false;
    }

    if (!bSeotdaBettingActive)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=BettingNotActive Player=%s Action=%d"),
            *RequestingPC->GetName(),
            static_cast<int32>(Action));
        return false;
    }

    AMainPlayerState* PS = RequestingPC->GetPlayerState<AMainPlayerState>();
    if (!PS)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=MissingPS Player=%s Action=%d"),
            *RequestingPC->GetName(),
            static_cast<int32>(Action));
        return false;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    if (TurnPS != PS)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=NotYourTurn Player=%s Turn=%s Action=%d"),
            *PS->GetPlayerName(),
            TurnPS ? *TurnPS->GetPlayerName() : TEXT("<NULL>"),
            static_cast<int32>(Action));
        return false;
    }

    FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
    if (!State || !State->bSubmitted || State->bFolded)
    {
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=InvalidState Player=%s Action=%d"),
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
            DS_LOG(TEXT("[DS] Seotda BetReject Reason=CheckNeedsCall Player=%s Call=%d"), *PS->GetPlayerName(), CallAmount);
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
        DS_LOG(TEXT("[DS] Seotda BetReject Reason=InvalidAction Player=%s Action=%d"),
            *PS->GetPlayerName(),
            static_cast<int32>(Action));
        return false;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SeotdaBetTurnTimeoutTimerHandle);
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
        State->bAllIn = GetSeotdaPlayerMoney(PS) <= 0;
        if (State->BetMoney > SeotdaCurrentBet)
        {
            SeotdaCurrentBet = State->BetMoney;
        }

        const bool bRaised = SeotdaCurrentBet > OldCurrentBet;
        if (bRaised)
        {
            for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
            {
                if (Pair.Key != PS &&
                    Pair.Value.bSubmitted &&
                    !Pair.Value.bFolded &&
                    !Pair.Value.bAllIn)
                {
                    Pair.Value.bActedThisBetRound = false;
                }
            }
        }

        State->bActedThisBetRound = true;
    }

    DS_LOG(TEXT("[DS] Seotda BetOK Player=%s Action=%d Paid=%d BetMoney=%d Pot=%d CurrentBet=%d Money=%d Folded=%d AllIn=%d"),
        *PS->GetPlayerName(),
        static_cast<int32>(Action),
        Paid,
        State->BetMoney,
        SeotdaPot,
        SeotdaCurrentBet,
        GetSeotdaPlayerMoney(PS),
        State->bFolded ? 1 : 0,
        State->bAllIn ? 1 : 0);

    if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
    {
        ResolveSeotdaRoundResult(TEXT("BetSettled"));
        BroadcastSeotdaState();

        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaBetSettled"));
        }

        return true;
    }

    AdvanceSeotdaBettingTurn();
    return true;
}

void UCardGameService::AdvanceSeotdaBettingTurn()
{
    if (SeotdaTurnOrder.Num() <= 0)
    {
        ResolveSeotdaRoundResult(TEXT("NoTurnOrder"));
        BroadcastSeotdaState();
        if (OwnerGM && OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaNoTurnOrderSettled"));
        }
        return;
    }

    for (int32 Step = 0; Step < SeotdaTurnOrder.Num(); ++Step)
    {
        SeotdaCurrentTurnIndex = (SeotdaCurrentTurnIndex + 1) % SeotdaTurnOrder.Num();
        AMainPlayerState* CandidatePS = SeotdaTurnOrder[SeotdaCurrentTurnIndex].Get();
        FSeotdaPlayerRoundState* State = CandidatePS ? SeotdaRoundStates.Find(CandidatePS) : nullptr;
        if (CandidatePS &&
            State &&
            State->bSubmitted &&
            !State->bFolded &&
            !State->bAllIn)
        {
            DS_LOG(TEXT("[DS] Seotda BetTurn Player=%s Index=%d Pot=%d CurrentBet=%d NeedCall=%d"),
                *CandidatePS->GetPlayerName(),
                SeotdaCurrentTurnIndex,
                SeotdaPot,
                SeotdaCurrentBet,
                FMath::Max(0, SeotdaCurrentBet - State->BetMoney));

            BroadcastSeotdaState();
            StartSeotdaBetTurnTimeout();

            return;
        }
    }

    ResolveSeotdaRoundResult(TEXT("NoActiveTurn"));
    BroadcastSeotdaState();
    if (OwnerGM && OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
    {
        OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaNoActiveTurnSettled"));
    }
}

void UCardGameService::ResolveSeotdaRoundResult(const TCHAR* Reason)
{
    if (!OwnerGM->HasAuthority())
    {
        return;
    }

    ClearSeotdaTimers();

    if (bSeotdaRoundResolved)
    {
        DS_LOG(TEXT("[DS] Seotda ResultSkip AlreadyResolved Summary=%s"),
            *LastSeotdaRoundResultSummary);
        return;
    }

    for (int32 RedealAttempt = 0; RedealAttempt < 8 && ShouldForceSeotdaRedeal(); ++RedealAttempt)
    {
        if (!TryApplySeotdaRedealFromRemainingCards(Reason))
        {
            DS_LOG(TEXT("[DS] Seotda RedealStop Reason=NotEnoughRemainingCards Attempt=%d"), RedealAttempt);
            break;
        }
    }

    TArray<FSeotdaPlayerRoundState*> WinningStates;

    for (TPair<AMainPlayerState*, FSeotdaPlayerRoundState>& Pair : SeotdaRoundStates)
    {
        FSeotdaPlayerRoundState& State = Pair.Value;

        if (!State.bSubmitted || State.bFolded)
        {
            continue;
        }

        if (WinningStates.IsEmpty())
        {
            WinningStates.Add(&State);
            continue;
        }

        const int32 CompareResult = FSeotdaRuleService::CompareSeotdaHands(
            State.HandResult,
            WinningStates[0]->HandResult);

        if (CompareResult > 0)
        {
            WinningStates.Reset();
            WinningStates.Add(&State);
        }
        else if (CompareResult == 0)
        {
            WinningStates.Add(&State);
        }
    }

    WinningStates.RemoveAll([](const FSeotdaPlayerRoundState* State)
    {
        return !State || !State->PlayerState.IsValid();
    });

    if (WinningStates.IsEmpty())
    {
        LastSeotdaRoundResultSummary = FString::Printf(
            TEXT("Winner=None Combo=None Pot=%d Reason=%s"),
            SeotdaPot,
            Reason ? Reason : TEXT("<NULL>")
        );

        bSeotdaBettingActive = false;
        bSeotdaRoundResolved = true;
        ClearReconnectSeotdaStateForRound(TEXT("ResultNoWinner"));
        BroadcastSeotdaState();

        DS_LOG(TEXT("[DS] Seotda ResultFailed %s"), *LastSeotdaRoundResultSummary);
        return;
    }

    TArray<AMainPlayerState*> WinnerPlayers;
    WinnerPlayers.Reserve(WinningStates.Num());
    for (FSeotdaPlayerRoundState* State : WinningStates)
    {
        if (AMainPlayerState* WinnerPS = State ? State->PlayerState.Get() : nullptr)
        {
            WinnerPlayers.AddUnique(WinnerPS);
        }
    }

    WinnerPlayers.Sort([](const AMainPlayerState& Left, const AMainPlayerState& Right)
    {
        const int32 NameCompare = Left.GetPlayerName().Compare(
            Right.GetPlayerName(),
            ESearchCase::IgnoreCase);
        if (NameCompare != 0)
        {
            return NameCompare < 0;
        }
        return Left.GetPlayerId() < Right.GetPlayerId();
    });

    const int32 WinnerCount = WinnerPlayers.Num();
    const int32 BaseShare = WinnerCount > 0 ? SeotdaPot / WinnerCount : 0;
    const int32 Remainder = WinnerCount > 0 ? SeotdaPot % WinnerCount : 0;

    TArray<FString> WinnerNames;
    TArray<FString> PayoutParts;
    for (int32 WinnerIndex = 0; WinnerIndex < WinnerCount; ++WinnerIndex)
    {
        AMainPlayerState* WinnerPS = WinnerPlayers[WinnerIndex];
        const int32 Award = BaseShare + (WinnerIndex < Remainder ? 1 : 0);
        if (Award > 0)
        {
            WinnerPS->AddGold(Award);
        }

        WinnerNames.Add(WinnerPS->GetPlayerName());
        PayoutParts.Add(FString::Printf(
            TEXT("%s:+%d=>%d"),
            *WinnerPS->GetPlayerName(),
            Award,
            GetSeotdaPlayerMoney(WinnerPS)));
    }

    FSeotdaPlayerRoundState* BestState = WinningStates[0];
    const bool bTie = WinnerCount > 1;
    const FString WinnerLabel = bTie
        ? FString::Printf(TEXT("Tie(%s)"), *FString::Join(WinnerNames, TEXT("/")))
        : WinnerNames[0];

    LastSeotdaRoundResultSummary = FString::Printf(
        TEXT("Winner=%s Combo=%s Rank=%d SubRank=%d Pot=%d Tie=%d Payouts=%s Reason=%s"),
        *WinnerLabel,
        *BestState->HandResult.Name,
        BestState->HandResult.Rank,
        BestState->HandResult.SubRank,
        SeotdaPot,
        bTie ? 1 : 0,
        *FString::Join(PayoutParts, TEXT(",")),
        Reason ? Reason : TEXT("<NULL>")
    );

    DS_LOG(TEXT("[DS] Seotda Winner %s"), *LastSeotdaRoundResultSummary);

    bSeotdaBettingActive = false;
    bSeotdaRoundResolved = true;
    ClearReconnectSeotdaStateForRound(TEXT("ResultResolved"));
    BroadcastSeotdaState();

    const FString ClientResultText = FString::Printf(
        TEXT("[ROUND %d RESULT] %s"),
        OwnerGM->GetCurrentRound(),
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
void UCardGameService::BroadcastSeotdaState() const
{
if (!OwnerGM->HasAuthority() || !GetWorld())
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

bool bMyRevealConfirmed = false;
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
bMyRevealConfirmed = State->bRevealConfirmed;
bMySubmitted = State->bSubmitted;
bMyFolded = State->bFolded;
MyBetMoney = State->BetMoney;
NeedCall = FMath::Max(0, SeotdaCurrentBet - MyBetMoney);
}
}

MPC->Client_UpdateSeotdaState(
OwnerGM->GetCurrentRound(),
bSeotdaBettingActive,
TurnName,
SeotdaPot,
SeotdaCurrentBet,
MyBetMoney,
NeedCall,
bMyTurn,
bMyRevealConfirmed,
bMySubmitted,
bMyFolded,
bSeotdaRoundResolved
);
}
}
AMainPlayerState* UCardGameService::GetCurrentSeotdaTurnPlayer() const
{
    if (SeotdaTurnOrder.Num() <= 0 || !SeotdaTurnOrder.IsValidIndex(SeotdaCurrentTurnIndex))
    {
        return nullptr;
    }

    return SeotdaTurnOrder[SeotdaCurrentTurnIndex].Get();
}

int32 UCardGameService::GetSeotdaPlayerMoney(const AMainPlayerState* TargetPS) const
{
    return TargetPS ? TargetPS->CurPlayerData.HoldingGold : 0;
}

int32 UCardGameService::PaySeotdaBet(AMainPlayerState* TargetPS, int32 Amount)
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

int32 UCardGameService::GetActiveSeotdaPlayerCount() const
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

bool UCardGameService::AreSeotdaBetsSettled() const
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

if (State.bAllIn)
{
ActedCount++;
continue;
}

if (!State.bActedThisBetRound)
{
UE_LOG(LogTemp, Verbose,
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
UE_LOG(LogTemp, Verbose,
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

UE_LOG(LogTemp, Verbose,
TEXT("[DS] Seotda SettleCheck Result=%d Active=%d Acted=%d CurrentBet=%d"),
bSettled ? 1 : 0,
ActiveSubmittedCount,
ActedCount,
SeotdaCurrentBet
);

return bSettled;
}

bool UCardGameService::TryPickupNearestCard(AMainPlayerController* RequestingPC)
{
    if (!OwnerGM->HasAuthority())
    {
        return false;
    }

    if (!RequestingPC)
    {
        DS_LOG(TEXT("[DS] Card PickupNearestReject Reason=InvalidRequest"));
        return false;
    }

    if (!IsCardPickupAllowed())
    {
        DS_LOG(TEXT("[DS] Card PickupNearestReject Reason=InvalidPhase Player=%s Phase=%s"),
            *RequestingPC->GetName(),
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()));
        return false;
    }

    APawn* Pawn = RequestingPC->GetPawn();
    if (!Pawn)
    {
        DS_LOG(TEXT("[DS] Card PickupNearestReject Reason=MissingPawn Player=%s"), *RequestingPC->GetName());
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
        DS_LOG(TEXT("[DS] Card PickupNearestReject Reason=NoNearbyCard Player=%s Range=%.2f"),
            *RequestingPC->GetName(),
            CardPickupRange);
        return false;
    }

    DS_LOG(TEXT("[DS] Card PickupNearest Player=%s Instance=%d Card=%d Name=%s Distance=%.2f"),
        *RequestingPC->GetName(),
        BestCard->GetCardInstanceId(),
        static_cast<int32>(BestCard->GetCardID()),
        *CardDebug::ToString(BestCard->GetCardID()),
        FMath::Sqrt(BestDistanceSq));

    return TryPickupCard(RequestingPC, BestCard);
}

int32 UCardGameService::CreateCardInstance(ECardID CardID)
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
    Record.CreatedRound = OwnerGM->GetCurrentRound();

    ServerCardRecords.Add(NewInstanceId, Record);

    DS_LOG(TEXT("[DS] Card Create Instance=%d Card=%d Name=%s Round=%d"),
        NewInstanceId,
        static_cast<int32>(CardID),
        *CardDebug::ToString(CardID),
        OwnerGM->GetCurrentRound());

    return NewInstanceId;
}

ACardDropActor* UCardGameService::SpawnCardDrop(ECardID CardID, const FVector& SpawnLocation)
{
    if (!OwnerGM->HasAuthority() || !GetWorld() || CardID == ECardID::None)
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
        UE_LOG(LogManagerCard, Error, TEXT("[DS] Card DropFail Card=%d Name=%s Reason=SpawnCollisionOrNull Location=%s"),
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

    const FVector ActualLocation = CardActor->GetActorLocation();
    const float AdjustedDistance = FVector::Dist(SpawnLocation, ActualLocation);
    const ULevel* ActorLevel = CardActor->GetLevel();
    const FString LevelPackage = ActorLevel && ActorLevel->GetOutermost()
        ? ActorLevel->GetOutermost()->GetName()
        : TEXT("<NO_LEVEL>");

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardSpawnFinal Instance=%d Card=%d Name=%s Actor=%s Class=%s Requested=%s Actual=%s AdjustedDistance=%.1f Level=%s Replicates=%d AlwaysRelevant=%d Hidden=%d Round=%d"),
        InstanceId,
        static_cast<int32>(CardID),
        *CardDebug::ToString(CardID),
        *CardActor->GetName(),
        *GetNameSafe(CardActor->GetClass()),
        *SpawnLocation.ToCompactString(),
        *ActualLocation.ToCompactString(),
        AdjustedDistance,
        *LevelPackage,
        CardActor->GetIsReplicated() ? 1 : 0,
        CardActor->bAlwaysRelevant ? 1 : 0,
        CardActor->IsHidden() ? 1 : 0,
        OwnerGM->GetCurrentRound());

    return CardActor;
}

ACardDropActor* UCardGameService::SpawnExistingOwnedCardDrop(
    const FServerCardRecord& Record,
    AActor* SourceActor,
    const FVector& SourceLocation)
{
    if (!OwnerGM || !OwnerGM->HasAuthority() || !GetWorld() ||
        Record.CardInstanceId <= 0 || Record.CardID == ECardID::None)
    {
        return nullptr;
    }

    FVector DropLocation = SourceLocation + FVector(0.0f, 0.0f, 80.0f);
    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardManualDropGround), false);
    if (SourceActor)
    {
        QueryParams.AddIgnoredActor(SourceActor);
    }

    const FVector TraceStart = SourceLocation + FVector(0.0f, 0.0f, 250.0f);
    const FVector TraceEnd = SourceLocation - FVector(0.0f, 0.0f, 2000.0f);
    if (GetWorld()->LineTraceSingleByChannel(
        GroundHit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams))
    {
        DropLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 80.0f);
    }

    TSubclassOf<ACardDropActor> SpawnClass = CardDropActorClass;
    if (!SpawnClass)
    {
        SpawnClass = ACardDropActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.Owner = SourceActor;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACardDropActor* CardActor = GetWorld()->SpawnActor<ACardDropActor>(
        SpawnClass,
        DropLocation,
        FRotator::ZeroRotator,
        Params);
    if (!CardActor)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Card ManualDropFail Instance=%d Card=%d Source=%s Requested=%s"),
            Record.CardInstanceId,
            static_cast<int32>(Record.CardID),
            *GetNameSafe(SourceActor),
            *DropLocation.ToCompactString());
        return nullptr;
    }

    CardActor->InitCardDrop(Record.CardInstanceId, Record.CardID);
    return CardActor;
}

bool UCardGameService::DiscardOwnedCard(
    AMainPlayerController* RequestingPC,
    int32 CardInstanceId,
    bool bDropIntoWorld,
    const TCHAR* Context)
{
    AMainPlayerState* TargetPS = RequestingPC
        ? RequestingPC->GetPlayerState<AMainPlayerState>()
        : nullptr;
    APawn* SourcePawn = RequestingPC ? RequestingPC->GetPawn() : nullptr;

    if (!OwnerGM || !OwnerGM->HasAuthority() || !TargetPS || CardInstanceId <= 0 ||
        (bDropIntoWorld && !SourcePawn))
    {
        return false;
    }

    const FOwnedCardInfo* OwnedCard = TargetPS->OwnedCards.FindByPredicate(
        [CardInstanceId](const FOwnedCardInfo& CardInfo)
        {
            return CardInfo.CardInstanceId == CardInstanceId;
        });
    FServerCardRecord* Record = ServerCardRecords.Find(CardInstanceId);

    if (!OwnedCard ||
        !Record ||
        Record->State != ECardRuntimeState::Owned ||
        Record->OwnerPlayerState.Get() != TargetPS ||
        Record->CardID != OwnedCard->CardID)
    {
        UE_LOG(LogManagerCard, Warning,
            TEXT("[DS][Security] CardDiscardReject Player=%s Instance=%d Reason=OwnershipRecordMismatch HasOwned=%d HasRecord=%d RecordState=%d RecordOwner=%s"),
            *TargetPS->GetPlayerName(),
            CardInstanceId,
            OwnedCard ? 1 : 0,
            Record ? 1 : 0,
            Record ? static_cast<int32>(Record->State) : INDEX_NONE,
            Record ? *GetNameSafe(Record->OwnerPlayerState.Get()) : TEXT("<NO_RECORD>"));
        return false;
    }

    ACardDropActor* SpawnedDrop = nullptr;
    if (bDropIntoWorld)
    {
        SpawnedDrop = SpawnExistingOwnedCardDrop(*Record, SourcePawn, SourcePawn->GetActorLocation());
        if (!SpawnedDrop)
        {
            return false;
        }
    }

    FOwnedCardInfo RemovedCard;
    if (!TargetPS->RemoveOwnedCardByInstanceId(CardInstanceId, RemovedCard))
    {
        if (SpawnedDrop)
        {
            SpawnedDrop->Destroy();
        }
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] CardDiscardFail Player=%s Instance=%d Reason=OwnedListMutationFailed"),
            *TargetPS->GetPlayerName(),
            CardInstanceId);
        return false;
    }

    Record->State = bDropIntoWorld ? ECardRuntimeState::WorldDrop : ECardRuntimeState::Discarded;
    Record->OwnerPlayerState = nullptr;
    Record->DropActor = SpawnedDrop;
    if (SpawnedDrop)
    {
        ActiveCardDrops.Add(SpawnedDrop);
    }

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardDiscard Player=%s Instance=%d Card=%d Name=%s Context=%s WorldDrop=%d Location=%s RemainingOwned=%d"),
        *TargetPS->GetPlayerName(),
        RemovedCard.CardInstanceId,
        static_cast<int32>(RemovedCard.CardID),
        *CardDebug::ToString(RemovedCard.CardID),
        Context ? Context : TEXT("<NULL>"),
        SpawnedDrop ? 1 : 0,
        SpawnedDrop ? *SpawnedDrop->GetActorLocation().ToCompactString() : TEXT("<REMOVED>"),
        TargetPS->PublicCardCount);
    return true;
}

bool UCardGameService::SpawnRoundCardBundleForBattleRoyale(TArray<int32>& OutSpawnedCardInstanceIds)
{
    OutSpawnedCardInstanceIds.Reset();

    if (!OwnerGM || !OwnerGM->HasAuthority() || !GetWorld())
    {
        UE_LOG(LogManagerCard, Error, TEXT("[DS] CardBundleSpawnRejected Reason=InvalidAuthorityOrWorld"));
        return false;
    }

    const FCardPlacementService CardPlacement = OwnerGM->MakeCardPlacementService();
    ClearCardDrops();

    TArray<FCardIslandDropZone> IslandDropZones = CardPlacement.FindCardIslandDropZones();
    TArray<TArray<ECardID>> IslandCardGroups = CardPlacement.BuildBalancedIslandCardGroups();

    if (IslandCardGroups.Num() == 0)
    {
        UE_LOG(LogManagerCard, Error, TEXT("[DS] CardBundlePlanRejected Reason=NoCardGroups"));
        return false;
    }

    if (IslandDropZones.Num() < IslandCardGroups.Num())
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] CardBundlePlanRejected Reason=NotEnoughDistinctIslandZones Found=%d Required=%d Result=NoPartialSpawn"),
            IslandDropZones.Num(), IslandCardGroups.Num());
        return false;
    }

    if (IslandDropZones.Num() != OwnerGM->GetCardIslandDropExpectedZoneCount())
    {
        DS_LOG(TEXT("[DS] Card IslandDropZoneCountWarning Found=%d Expected=%d"),
            IslandDropZones.Num(),
            OwnerGM->GetCardIslandDropExpectedZoneCount());
    }

    if (IslandDropZones.Num() > IslandCardGroups.Num())
    {
        DS_LOG(TEXT("[DS] Card IslandDropExtraZonesIgnored Found=%d Used=%d"),
            IslandDropZones.Num(),
            IslandCardGroups.Num());
    }

    struct FPlannedCardDrop
    {
        ECardID CardID = ECardID::None;
        FVector Location = FVector::ZeroVector;
        ECardDropPlacementSource PlacementSource = ECardDropPlacementSource::None;
        int32 IslandIndex = INDEX_NONE;
        int32 SlotIndex = INDEX_NONE;
        int32 ZoneIndex = INDEX_NONE;
    };

    int32 PlannedCount = 0;
    for (const TArray<ECardID>& CardGroup : IslandCardGroups)
    {
        PlannedCount += CardGroup.Num();
    }

    int32 NavigationPlacedCount = 0;
    int32 FallbackPlacedCount = 0;
    int32 UnknownSourcePlacedCount = 0;
    int32 LocationFailedCount = 0;
    int32 SpawnFailedCount = 0;
    TMap<int32, TArray<FVector>> ExistingLocationsByZoneIndex;
    TArray<FPlannedCardDrop> PlannedDrops;
    PlannedDrops.Reserve(PlannedCount);

    for (int32 IslandIndex = 0; IslandIndex < IslandCardGroups.Num(); ++IslandIndex)
    {
        const TArray<ECardID>& CardsInIsland = IslandCardGroups[IslandIndex];
        const int32 ZoneIndex = IslandIndex;

        const FCardIslandDropZone& DropZone = IslandDropZones[ZoneIndex];
        const int32 BalanceValue = CardPlacement.GetCardIslandGroupBalanceValue(CardsInIsland);

        AActor* ZoneActor = DropZone.ZoneActor.Get();
        DS_LOG(TEXT("[DS] Card IslandGroup Island=%d ZoneIndex=%d Zone=%s Key=%s Source=%s Cards=%d BalanceValue=%d BoundsCenter=%s BoundsExtent=%s"),
            IslandIndex,
            ZoneIndex,
            ZoneActor ? *ZoneActor->GetName() : TEXT("None"),
            *DropZone.IslandKey.ToString(),
            *DropZone.Source,
            CardsInIsland.Num(),
            BalanceValue,
            *DropZone.Center.ToString(),
            *DropZone.Bounds.GetExtent().ToString());

        TArray<FVector>& ExistingIslandLocations = ExistingLocationsByZoneIndex.FindOrAdd(ZoneIndex);
        int32 IslandPlaced = 0;

        for (int32 SlotIndex = 0; SlotIndex < CardsInIsland.Num(); ++SlotIndex)
        {
            const ECardID CardID = CardsInIsland[SlotIndex];

            FVector SpawnLocation = FVector::ZeroVector;
            ECardDropPlacementSource PlacementSource = ECardDropPlacementSource::None;
            const bool bPicked = CardPlacement.PickIslandCardDropLocation(
                DropZone,
                ExistingIslandLocations,
                IslandIndex,
                SlotIndex,
                SpawnLocation,
                &PlacementSource);

            if (!bPicked)
            {
                ++LocationFailedCount;
                // 자리를 못 잡으면 겹쳐 놓지 않고 이 카드는 건너뛴다. 실패 사유는 PickIslandCardDropLocation 로그에 남는다.
                UE_LOG(LogManagerCard, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Card=%d Name=%s Reason=NoValidLocation Result=RejectWholeBundle"),
                    IslandIndex, SlotIndex, static_cast<int32>(CardID), *CardDebug::ToString(CardID));
                continue;
            }

            ExistingIslandLocations.Add(SpawnLocation);
            FPlannedCardDrop& PlannedDrop = PlannedDrops.AddDefaulted_GetRef();
            PlannedDrop.CardID = CardID;
            PlannedDrop.Location = SpawnLocation;
            PlannedDrop.PlacementSource = PlacementSource;
            PlannedDrop.IslandIndex = IslandIndex;
            PlannedDrop.SlotIndex = SlotIndex;
            PlannedDrop.ZoneIndex = ZoneIndex;
            ++IslandPlaced;

            switch (PlacementSource)
            {
            case ECardDropPlacementSource::Navigation:
                ++NavigationPlacedCount;
                break;
            case ECardDropPlacementSource::GroundTraceFallback:
                ++FallbackPlacedCount;
                break;
            default:
                ++UnknownSourcePlacedCount;
                break;
            }
        }

        UE_LOG(LogManagerCard, Verbose, TEXT("[DS] Card IslandDropSummary Island=%d ZoneIndex=%d Zone=%s Key=%s Source=%s Placed=%d Requested=%d Center=%s Extent=%s"),
            IslandIndex, ZoneIndex, ZoneActor ? *ZoneActor->GetName() : TEXT("None"), *DropZone.IslandKey.ToString(), *DropZone.Source,
            IslandPlaced, CardsInIsland.Num(), *DropZone.Center.ToCompactString(), *DropZone.Bounds.GetExtent().ToCompactString());
    }

    if (LocationFailedCount > 0 || PlannedDrops.Num() != PlannedCount)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] CardBundlePlanRejected Reason=IncompletePlacement PlannedCards=%d Locations=%d LocationFailed=%d Zones=%d Islands=%d Result=NoActorsSpawned"),
            PlannedCount,
            PlannedDrops.Num(),
            LocationFailedCount,
            IslandDropZones.Num(),
            IslandCardGroups.Num());
        return false;
    }

    for (const FPlannedCardDrop& PlannedDrop : PlannedDrops)
    {
        ACardDropActor* SpawnedCard = SpawnCardDrop(PlannedDrop.CardID, PlannedDrop.Location);
        if (!SpawnedCard)
        {
            ++SpawnFailedCount;
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] CardBundleCommitFailed Island=%d Slot=%d ZoneIndex=%d Card=%d Name=%s Reason=SpawnActorFailed"),
                PlannedDrop.IslandIndex,
                PlannedDrop.SlotIndex,
                PlannedDrop.ZoneIndex,
                static_cast<int32>(PlannedDrop.CardID),
                *CardDebug::ToString(PlannedDrop.CardID));
            break;
        }

        OutSpawnedCardInstanceIds.Add(SpawnedCard->GetCardInstanceId());
    }

    if (SpawnFailedCount > 0
        || OutSpawnedCardInstanceIds.Num() != PlannedCount
        || ActiveCardDrops.Num() != PlannedCount)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] CardBundleCommitRollback Planned=%d SpawnedIds=%d ActiveActors=%d SpawnFailed=%d Result=DestroyAttemptActors"),
            PlannedCount,
            OutSpawnedCardInstanceIds.Num(),
            ActiveCardDrops.Num(),
            SpawnFailedCount);
        ClearCardDrops();
        for (const int32 FailedInstanceId : OutSpawnedCardInstanceIds)
        {
            ServerCardRecords.Remove(FailedInstanceId);
        }
        OutSpawnedCardInstanceIds.Reset();
        return false;
    }

    UE_LOG(LogManagerCard, Display,
        TEXT("[DS] CardSpawnSummary Planned=%d LocationFound=%d Spawned=%d NavPlaced=%d FallbackPlaced=%d UnknownSource=%d LocationFailed=%d SpawnFailed=%d Zones=%d Islands=%d Round=%d Phase=%s"),
        PlannedCount,
        PlannedDrops.Num(),
        ActiveCardDrops.Num(),
        NavigationPlacedCount,
        FallbackPlacedCount,
        UnknownSourcePlacedCount,
        LocationFailedCount,
        SpawnFailedCount,
        IslandDropZones.Num(),
        IslandCardGroups.Num(),
        OwnerGM->GetCurrentRound(),
        OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()));

    return true;
}

void UCardGameService::ClearCardDrops()
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
        DS_LOG(TEXT("[DS] Card ClearDrops Count=%d Round=%d"), ClearCount, OwnerGM->GetCurrentRound());
    }
}


void UCardGameService::EnsureThreeCardsForCardGame()
{
    if (!OwnerGM->HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<int32> SupplementRecordIds;
    for (const TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        const FServerCardRecord& Record = Pair.Value;
        if (Record.CreatedRound == OwnerGM->GetCurrentRound() && Record.State == ECardRuntimeState::Removed && Record.CardID != ECardID::None)
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

        DS_LOG(TEXT("[DS] Card EnsureThree Player=%s Count=%d Max=%d Round=%d Cards=[%s]"),
            *PS->GetPlayerName(),
            PS->OwnedCards.Num(),
            TargetCardCount,
            OwnerGM->GetCurrentRound(),
            *GetOwnedCardsDebugString(PS));

        TargetCount++;
    }

    DS_LOG(TEXT("[DS] Card EnsureThreeComplete targets=%d round=%d"), TargetCount, OwnerGM->GetCurrentRound());
}

void UCardGameService::ClearRoundCardsForAllPlayers()
{
    if (!OwnerGM->HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearSeotdaTimers();

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

    DS_LOG(TEXT("[DS] Card ClearRoundCards targets=%d cards=%d round=%d"), TargetCount, CardCount, OwnerGM->GetCurrentRound());
}

int32 UCardGameService::ClearDetachedOwnedCardRecords(
    const TArray<FOwnedCardInfo>& OwnedCards,
    const TCHAR* Context)
{
    if (!OwnerGM || !OwnerGM->HasAuthority())
    {
        return 0;
    }

    int32 ClearedCount = 0;
    for (const FOwnedCardInfo& CardInfo : OwnedCards)
    {
        FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId);
        if (!Record || Record->CardID != CardInfo.CardID)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Card DetachedClearMismatch Instance=%d Card=%d HasRecord=%d RecordCard=%d Context=%s"),
                CardInfo.CardInstanceId,
                static_cast<int32>(CardInfo.CardID),
                Record ? 1 : 0,
                Record ? static_cast<int32>(Record->CardID) : INDEX_NONE,
                Context ? Context : TEXT("<NULL>"));
            continue;
        }

        if (Record->State != ECardRuntimeState::Owned &&
            Record->State != ECardRuntimeState::Used &&
            Record->State != ECardRuntimeState::Removed)
        {
            UE_LOG(LogManagerCard, Warning,
                TEXT("[DS] Card DetachedClearUnexpectedState Instance=%d Card=%d State=%d Context=%s"),
                CardInfo.CardInstanceId,
                static_cast<int32>(CardInfo.CardID),
                static_cast<int32>(Record->State),
                Context ? Context : TEXT("<NULL>"));
        }

        if (ACardDropActor* DropActor = Record->DropActor.Get())
        {
            ActiveCardDrops.RemoveAll([DropActor](const TObjectPtr<ACardDropActor>& CardActor)
            {
                return CardActor.Get() == DropActor;
            });
            DropActor->Destroy();
        }

        Record->State = ECardRuntimeState::Removed;
        Record->OwnerPlayerState = nullptr;
        Record->DropActor = nullptr;
        ++ClearedCount;
    }

    if (ClearedCount > 0)
    {
        UE_LOG(LogManagerCard, Display,
            TEXT("[DS] Card DetachedRoundCardsCleared Count=%d Requested=%d Round=%d Context=%s"),
            ClearedCount,
            OwnedCards.Num(),
            OwnerGM->GetCurrentRound(),
            Context ? Context : TEXT("<NULL>"));
    }
    return ClearedCount;
}

bool UCardGameService::GrantCardRecordToPlayer(int32 CardInstanceId, AMainPlayerState* TargetPS, const TCHAR* Context)
{
    if (!OwnerGM->HasAuthority() || !TargetPS || CardInstanceId <= 0)
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

    DS_LOG(TEXT("[DS] Card Grant Player=%s Instance=%d Card=%d Context=%s Count=%d"),
        *TargetPS->GetPlayerName(),
        CardInfo.CardInstanceId,
        static_cast<int32>(CardInfo.CardID),
        Context ? Context : TEXT("<NULL>"),
        TargetPS->PublicCardCount);

    return true;
}

bool UCardGameService::GrantNewCardToPlayer(AMainPlayerState* TargetPS, ECardID CardID, const TCHAR* Context)
{
    if (!OwnerGM->HasAuthority() || !TargetPS || CardID == ECardID::None)
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

ECardID UCardGameService::PickSupplementCardIDForPlayer(const AMainPlayerState* TargetPS) const
{
    const FCardPlacementService CardPlacement = OwnerGM->MakeCardPlacementService();
    TArray<ECardID> CardIDs = CardPlacement.BuildCardBundleIDs();
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
        CardIDs = CardPlacement.BuildCardBundleIDs();
    }

    if (CardIDs.Num() == 0)
    {
        return ECardID::None;
    }

    return CardIDs[FMath::RandRange(0, CardIDs.Num() - 1)];
}

bool UCardGameService::IsCardPickupAllowed() const
{
    return OwnerGM->GetCurrentServerPhase() == EDediServerPhase::BattleRoyale;
}

bool UCardGameService::ShouldForceSeotdaRedeal() const
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

            if (!bHasOpponent || FSeotdaRuleService::CompareSeotdaHands(OtherState.HandResult, BestOpponentResult) > 0)
            {
                bHasOpponent = true;
                BestOpponentResult = OtherState.HandResult;
                BestOpponentName = OtherPS->GetPlayerName();
            }
        }

        if (!bHasOpponent)
        {
            DS_LOG(TEXT("[DS] Seotda RedealRule Skip Player=%s Rule=%s Reason=NoActiveOpponent"),
                *RedealPS->GetPlayerName(),
                *RedealState.HandResult.Name);
            continue;
        }

        // ?쇰컲 援ъ궗: ?곷? 理쒓퀬 議깅낫媛 ?뚮━ ?댄븯?대㈃ ?ш꼍湲?
        // 硫띻뎄?? ?곷? 理쒓퀬 議깅낫媛 9???댄븯?대㈃ ?ш꼍湲?
        const int32 AllowedMaxRank = bIsMeongGusa ? 10009 : 9000;
        const bool bAllowRedeal = BestOpponentResult.Rank <= AllowedMaxRank;

        DS_LOG(TEXT("[DS] Seotda RedealRule Check Player=%s Rule=%s Opponent=%s OpponentCombo=%s OpponentRank=%d AllowedMaxRank=%d Redeal=%d"),
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
bool UCardGameService::TryApplySeotdaRedealFromRemainingCards(const TCHAR* Reason)
{
    if (!OwnerGM->HasAuthority() || !GetWorld())
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

        if (Record.CreatedRound == OwnerGM->GetCurrentRound() &&
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
        DS_LOG(TEXT("[DS] Seotda RedealReject Reason=NotEnoughCards Need=%d Remain=%d Round=%d"),
            NeedCardCount,
            RemainingRecordIds.Num(),
            OwnerGM->GetCurrentRound());
        return false;
    }

    DS_LOG(TEXT("[DS] Seotda RedealStart Reason=%s ActivePlayers=%d NeedCards=%d RemainCards=%d Round=%d Pot=%d"),
        Reason ? Reason : TEXT("<NULL>"),
        ActivePlayers.Num(),
        NeedCardCount,
        RemainingRecordIds.Num(),
        OwnerGM->GetCurrentRound(),
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
        State->HandResult = FSeotdaRuleService::EvaluateSeotdaHand(FirstInfo, SecondInfo);
        State->bSubmitted = true;
        State->bActedThisBetRound = true;

        DS_LOG(TEXT("[DS] Seotda RedealCard Player=%s Cards=%d:%s,%d:%s Combo=%s Rank=%d SubRank=%d"),
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

            DS_LOG(TEXT("[DS] Seotda RedealNotice Player=%s Text=%s"),
                *PS->GetPlayerName(),
                *RedealNotice);

            MPC->Client_ShowSeotdaResult(RedealNotice);
            break;
        }
    }

    return true;
}

