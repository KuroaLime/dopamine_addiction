#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/InGame/Interface/PhaseGameModeInterface.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "MainGameMode.generated.h"

class UPhaseStrategy;
class APlayerController;
class AController;
class AMainPlayerController;
class AMainPlayerState;
class ACardDropActor;
class USpawnManagerComponent;

enum class EDediServerPhase : uint8
{
    None,
    Ready,
    BattleRoyale,
    TransitionToCard,
    CardGame,
    Result,
    TransitionToBattle,
    GameEnd
};

UCLASS()
class MANAGER_API AMainGameMode : public AGameModeBase,
                      public IPhaseGameModeInterface
{
    GENERATED_BODY()

public:
    AMainGameMode();

    virtual void InitGame(
        const FString& MapName,
        const FString& Options,
        FString& ErrorMessage) override;

    virtual void PreLogin(
        const FString& Options,
        const FString& Address,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage) override;

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

public:
    virtual void BeginPhase(EGamePhase CurrPhase) override;
    virtual void EndPhase() override;
    virtual void ChangePhase(EGamePhase NewPhase) override;
    virtual void BroadcastSwitchMode(EGamePhase NewPhase) override;
    virtual void BroadcastSwitchLevel(FName LevelToUnload, FName LevelToLoad) override;

public:
    void OnPlayerAction(AActor* Executor, FName ActionName);
    bool IsBattleRoyalePhase() const;
    bool TryPickupCard(AMainPlayerController* RequestingPC, ACardDropActor* TargetCard);
    bool TryPickupNearestCard(AMainPlayerController* RequestingPC);
    bool SubmitSeotdaSelection(AMainPlayerController* RequestingPC, bool bCard0, bool bCard1, bool bCard2);
    bool SubmitSeotdaBetAction(AMainPlayerController* RequestingPC, EBettingAction Action);

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Setup")
    TMap<EGamePhase, TSubclassOf<UPhaseStrategy>> StrategyClassMap;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Setup")
    EGamePhase InitialPhase = EGamePhase::TPS;

    UPROPERTY(BlueprintReadOnly, Category = "Dedicated Server")
    int32 DediRoomId = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dedicated Server")
    int32 RequiredPlayerCount = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Dedicated Server")
    bool bGameStarted = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Config")
    bool bUseDebugPhaseDurations = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Config")
    int32 MaxRoundCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Real")
    int32 RealReadyDuration = 60;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Real")
    int32 RealBattleRoyaleDuration = 300;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Real")
    int32 RealTransitionDuration = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Real")
    int32 RealCardGameDuration = 120;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Real")
    int32 RealResultDuration = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Debug")
    int32 DebugReadyDuration = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Debug")
    int32 DebugBattleRoyaleDuration = 60;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Debug")
    int32 DebugTransitionDuration = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Debug")
    int32 DebugCardGameDuration = 20;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Debug")
    int32 DebugResultDuration = 5;

    UPROPERTY(BlueprintReadOnly, Category = "Phase|State")
    int32 CurrentRound = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Phase|State")
    int32 RemainingPhaseSeconds = 0;


    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Server")
    TSubclassOf<ACardDropActor> CardDropActorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Server")
    float CardPickupRange = 350.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Server")
    int32 MaxCardsPerPlayerPerRound = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Bundle")
    FVector CardBundleDropCenter = FVector(0.0f, 0.0f, 180.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Bundle")
    FVector2D CardBundleDropExtent = FVector2D(1200.0f, 800.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Bundle")
    float CardBundleDropJitterRatio = 0.3f;

    UPROPERTY()
    TMap<EGamePhase, TObjectPtr<UPhaseStrategy>> StrategyMap;

    UPROPERTY()
    UPhaseStrategy* CurrentStrategy;

private:
    FTimerHandle PhaseTimerHandle;
    EDediServerPhase CurrentServerPhase = EDediServerPhase::None;
    bool bGameEndReached = false;

    struct FServerCardRecord
    {
        int32 CardInstanceId = 0;
        ECardID CardID = ECardID::None;
        ECardRuntimeState State = ECardRuntimeState::None;
        TWeakObjectPtr<AMainPlayerState> OwnerPlayerState;
        TWeakObjectPtr<ACardDropActor> DropActor;
        int32 CreatedRound = 0;
    };

    int32 NextCardInstanceId = 1;
    TMap<int32, FServerCardRecord> ServerCardRecords;

    UPROPERTY()
    TArray<TObjectPtr<ACardDropActor>> ActiveCardDrops;


    struct FSeotdaHandResult
    {
        int32 Rank = 0;
        int32 SubRank = 0;
        FString Name;
        TArray<int32> UsedCardInstanceIds;
    };

    struct FSeotdaPlayerRoundState
    {
        TWeakObjectPtr<AMainPlayerState> PlayerState;
        TArray<int32> SelectedCardInstanceIds;
        FSeotdaHandResult HandResult;
        bool bSubmitted = false;
        bool bFolded = false;
        bool bActedThisBetRound = false;
        int32 BetMoney = 0;
    };

    TMap<AMainPlayerState*, FSeotdaPlayerRoundState> SeotdaRoundStates;
    TArray<TWeakObjectPtr<AMainPlayerState>> SeotdaTurnOrder;
    int32 SeotdaPot = 0;
    int32 SeotdaCurrentBet = 0;
    int32 SeotdaCurrentTurnIndex = 0;
    bool bSeotdaBettingActive = false;


private:
    void InitStrategy();
    void TryStartGameIfReady();
    int32 CountConnectedHumanPlayers() const;

    void StartReadyPhase();
    void StartBattleRoyalePhase();
    void StartTransitionToCardPhase();
    void StartCardGamePhase();
    void StartResultPhase();
    void StartTransitionToBattlePhase();
    void StartGameEndPhase();

    void SetPlayerPawnGameplayEnabled(bool bEnabled, const TCHAR* Context);
    void ClearPlayerPawnMovementBases(const TCHAR* Context);

    void StartTimedServerPhase(EDediServerPhase NewPhase, int32 DurationSeconds);
    void OnServerPhaseTick();
    void FinishCurrentServerPhase(const TCHAR* Reason);
    void ClearServerPhaseTimer();
    void SetServerRemainingTime(int32 NewTime);

    TArray<ECardID> BuildCardBundleIDs() const;
    void ShuffleCardIDs(TArray<ECardID>& CardIDs) const;
    FVector GetDistributedCardDropLocation(int32 Index, int32 TotalCount) const;
    int32 CreateCardInstance(ECardID CardID);
    ACardDropActor* SpawnCardDrop(ECardID CardID, const FVector& SpawnLocation);
    void SpawnRoundCardBundleForBattleRoyale();
    void ClearCardDrops();

    void EnsureThreeCardsForCardGame();
    void ClearRoundCardsForAllPlayers();
    bool GrantCardRecordToPlayer(int32 CardInstanceId, AMainPlayerState* TargetPS, const TCHAR* Context);
    bool GrantNewCardToPlayer(AMainPlayerState* TargetPS, ECardID CardID, const TCHAR* Context);
    ECardID PickSupplementCardIDForPlayer(const AMainPlayerState* TargetPS) const;
    bool IsCardPickupAllowed() const;

    void ResetSeotdaRoundStates();
    void TryResolveSeotdaRoundIfReady();
    void StartSeotdaBettingRound();
    void AdvanceSeotdaBettingTurn();
    void ResolveSeotdaRoundResult(const TCHAR* Reason);
    AMainPlayerState* GetCurrentSeotdaTurnPlayer() const;
    int32 GetSeotdaPlayerMoney(const AMainPlayerState* TargetPS) const;
    int32 PaySeotdaBet(AMainPlayerState* TargetPS, int32 Amount);
    int32 GetActiveSeotdaPlayerCount() const;
    bool AreSeotdaBetsSettled() const;
    FSeotdaHandResult EvaluateSeotdaHand(const FOwnedCardInfo& FirstCard, const FOwnedCardInfo& SecondCard) const;
    int32 GetSeotdaCardMonth(ECardID CardID) const;
    bool IsSeotdaGwang(ECardID CardID) const;
    bool HasSeotdaMonths(int32 FirstMonth, int32 SecondMonth, int32 A, int32 B) const;

    int32 GetReadyDuration() const;
    int32 GetBattleRoyaleDuration() const;
    int32 GetTransitionDuration() const;
    int32 GetCardGameDuration() const;
    int32 GetResultDuration() const;
    const TCHAR* GetServerPhaseName(EDediServerPhase Phase) const;

    public:
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameMode|Spawn")
        USpawnManagerComponent* SpawnManager;
        virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
        virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};
