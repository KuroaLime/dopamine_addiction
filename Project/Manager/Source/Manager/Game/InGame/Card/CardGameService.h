// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "Game/InGame/Card/SeotdaRuleService.h"
#include "CardGameService.generated.h"

class AMainGameMode;
class AMainPlayerState;
class AMainPlayerController;
class ACardDropActor;
class AActor;

// 서버 권위 카드 1장의 런타임 기록. 기존 AMainGameMode 내부 private 구조체에서 이전.
struct FServerCardRecord
{
    int32 CardInstanceId = 0;
    ECardID CardID = ECardID::None;
    ECardRuntimeState State = ECardRuntimeState::None;
    TWeakObjectPtr<AMainPlayerState> OwnerPlayerState;
    TWeakObjectPtr<ACardDropActor> DropActor;
    int32 CreatedRound = 0;
};

// 섯다 한 라운드에서 플레이어별 상태(공개 카드/최종 2장 패/베팅).
struct FSeotdaPlayerRoundState
{
    TWeakObjectPtr<AMainPlayerState> PlayerState;
    // 공개 카드는 정보 공개용이다. 최종 2장 패에는 포함될 수도, 제외될 수도 있다.
    int32 RevealedCardInstanceId = 0;
    bool bRevealConfirmed = false;
    // 최종 제출로 확정된 2장만 저장한다.
    TArray<int32> SelectedCardInstanceIds;
    FSeotdaHandResult HandResult;
    bool bSubmitted = false;
    bool bFolded = false;
    bool bAllIn = false;
    bool bActedThisBetRound = false;
    int32 BetMoney = 0;
};

/**
 * 카드게임 도메인 서비스 — 카드 인스턴스/드롭 관리, 섯다 카드 선택/베팅/라운드 진행 상태와 로직을 소유한다.
 * 기존 AMainGameMode에 몰려 있던 카드/섯다 상태+메서드를 이전한 것으로 동작은 동일하다.
 * GameMode(Context)가 소유하며, 페이즈 머신/매치 상태(CurrentRound 등)·레벨 스트리밍은 OwnerGM을 통해 접근한다.
 * 복제는 기존과 동일하게 PlayerState/GameState/PC Client RPC로 처리(서비스 자체는 서버 전용, 비복제).
 */
UCLASS()
class MANAGER_API UCardGameService : public UObject
{
    GENERATED_BODY()

public:
    // GameMode가 생성 직후 호출: 소유자 연결 + BP 설정값 복사.
    void Init(AMainGameMode* InOwner);

    // UObject는 기본적으로 World가 없으므로 소유 GameMode의 World를 노출한다(본문의 GetWorld() 동작 보존).
    virtual UWorld* GetWorld() const override;

    // ----- 외부(PC RPC)에서 호출되는 진입점 -----
    bool TryPickupCard(AMainPlayerController* RequestingPC, ACardDropActor* TargetCard);
    bool TryPickupNearestCard(AMainPlayerController* RequestingPC);
    bool DiscardOwnedCard(
        AMainPlayerController* RequestingPC,
        int32 CardInstanceId,
        bool bDropIntoWorld,
        const TCHAR* Context);
    bool RevealSeotdaCard(
        AMainPlayerController* RequestingPC,
        bool bCard0,
        bool bCard1,
        bool bCard2,
        FString& OutFailureReason);
    bool SubmitSeotdaSelection(
        AMainPlayerController* RequestingPC,
        bool bCard0,
        bool bCard1,
        bool bCard2,
        FString& OutFailureReason);
    bool SubmitSeotdaBetAction(AMainPlayerController* RequestingPC, EBettingAction Action);

    // ----- 페이즈/매치 흐름에서 호출 -----
    bool SpawnRoundCardBundleForBattleRoyale(TArray<int32>& OutSpawnedCardInstanceIds);
    void EnsureThreeCardsForCardGame();
    void ResetSeotdaRoundStates();
    void StartSeotdaSelectionTimeout();
    void ClearCardDrops();
    void ClearRoundCardsForAllPlayers();
    int32 ClearDetachedOwnedCardRecords(const TArray<FOwnedCardInfo>& OwnedCards, const TCHAR* Context);
    void ResolveSeotdaRoundResult(const TCHAR* Reason);
    void DetachPlayerForReconnect(int64 Ticket, AMainPlayerState* PlayerState);
    void ReattachPlayerAfterReconnect(int64 Ticket, AMainPlayerState* PlayerState);
    void ExpireReconnectState(int64 Ticket, const TCHAR* Reason);
    void HandlePlayerDisconnectedAfterLogout(int64 Ticket, const TCHAR* Reason);

    ACardDropActor* SpawnCardDrop(ECardID CardID, const FVector& SpawnLocation);

    // ----- GameMode가 결과/상태를 조회 -----
    int32 GetSeotdaPlayerMoney(const AMainPlayerState* TargetPS) const;
    bool IsRoundResolved() const { return bSeotdaRoundResolved; }
    int32 GetRoundStateCount() const { return SeotdaRoundStates.Num(); }
    const FString& GetLastRoundResultSummary() const { return LastSeotdaRoundResultSummary; }

private:
    int32 CreateCardInstance(ECardID CardID);
    bool GrantCardRecordToPlayer(int32 CardInstanceId, AMainPlayerState* TargetPS, const TCHAR* Context);
    bool GrantNewCardToPlayer(AMainPlayerState* TargetPS, ECardID CardID, const TCHAR* Context);
    ECardID PickSupplementCardIDForPlayer(const AMainPlayerState* TargetPS) const;
    ACardDropActor* SpawnExistingOwnedCardDrop(
        const FServerCardRecord& Record,
        AActor* SourceActor,
        const FVector& SourceLocation);
    bool IsCardPickupAllowed() const;
    FString GetOwnedCardsDebugString(const AMainPlayerState* PS) const;

    void TryResolveSeotdaRoundIfReady();
    void StartSeotdaBettingRound();
    void AdvanceSeotdaBettingTurn();
    void StartSeotdaBetTurnTimeout();
    void HandleSeotdaSelectionTimeout();
    void HandleSeotdaBetTurnTimeout();
    void ClearSeotdaTimers();
    void BroadcastSeotdaState() const;
    AMainPlayerState* GetCurrentSeotdaTurnPlayer() const;
    int32 PaySeotdaBet(AMainPlayerState* TargetPS, int32 Amount);
    int32 GetActiveSeotdaPlayerCount() const;
    bool AreSeotdaBetsSettled() const;
    void ClearReconnectSeotdaStateForRound(const TCHAR* Reason);
    bool ShouldForceSeotdaRedeal() const;
    bool TryApplySeotdaRedealFromRemainingCards(const TCHAR* Reason);

private:
    UPROPERTY()
    AMainGameMode* OwnerGM = nullptr;

    // ----- BP 설정값 복사본(Init에서 OwnerGM으로부터 채움) -----
    float CardPickupRange = 350.0f;
    int32 MaxCardsPerPlayerPerRound = 3;
    TSubclassOf<ACardDropActor> CardDropActorClass;
    int32 SeotdaServerSeedPot = 2;
    int32 SeotdaBaseCallBet = 2;

    // ----- 런타임 상태 -----
    int32 NextCardInstanceId = 1;
    TMap<int32, FServerCardRecord> ServerCardRecords;

    UPROPERTY()
    TArray<TObjectPtr<ACardDropActor>> ActiveCardDrops;

    TMap<AMainPlayerState*, FSeotdaPlayerRoundState> SeotdaRoundStates;
    TArray<TWeakObjectPtr<AMainPlayerState>> SeotdaTurnOrder;
    TMap<int64, FSeotdaPlayerRoundState> ReconnectSeotdaStates;
    TMap<int64, TArray<int32>> ReconnectTurnOrderIndices;
    int32 SeotdaPot = 0;
    int32 SeotdaCurrentBet = 0;
    int32 SeotdaCurrentTurnIndex = 0;
    bool bSeotdaBettingActive = false;
    bool bSeotdaRoundResolved = false;
    FString LastSeotdaRoundResultSummary = TEXT("Pending");
    FTimerHandle SeotdaSelectionTimeoutTimerHandle;
    FTimerHandle SeotdaBetTurnTimeoutTimerHandle;
};
