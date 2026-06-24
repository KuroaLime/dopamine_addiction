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
    float CardBundleDropJitterRatio = 0.1f;

    // 留듭뿉 諛곗튂??TriggerBox/BoxActor ?깆뿉 ??Actor Tag瑜?遺숈씠硫?
    // ?대떦 ?≫꽣??Bounds ?덉뿉???щ퀎 移대뱶 ?쒕엻 ?꾩튂瑜?戮묐뒗??
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    FName CardIslandDropZoneTag = TEXT("CardIslandDropZone");

    // ?뚯뒪??留듭쿂??BPP_MAP_Summer/Spring/Autumn/Winter PackedLevelActor瑜?
    // ?꾩떆 ?쒕엻 ?곸뿭?쇰줈 ?먮룞 ?몄떇?좎? ?щ?.
    // ?ㅼ쟾?먯꽌??TriggerBox DropZone???곕뒗 履쎌씠 ???덉쟾?섎떎.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    bool bAutoDetectSeasonIslandActorsAsDropZones = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    int32 CardIslandDropExpectedZoneCount = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    int32 CardIslandDropMaxAttemptsPerCard = 160;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandGroundTraceHalfHeight = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandGroundOffsetZ = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandMinCardDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    bool bProjectCardDropsToNavigation = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    FVector CardIslandNavProjectExtent = FVector(200.0f, 200.0f, 500.0f);

    // ??媛곷룄蹂대떎 媛?뚮Ⅸ ?쒕㈃?먮뒗 移대뱶瑜??볦? ?딅뒗??
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandMaxGroundSlopeDegrees = 35.0f;

    // 移대뱶媛 李⑥??섎뒗 ??듭쟻??怨듦컙. ??諛뺤뒪媛 吏?뺣Ъ/?μ븷臾쇨낵 寃뱀튂硫??대떦 ?꾩튂??踰꾨┛??
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    FVector CardIslandOverlapBoxExtent = FVector(80.0f, 80.0f, 60.0f);

    // CardNoDropZone 태그가 붙은 액터 Bounds 안에는 카드를 올리지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    FName CardNoDropZoneTag = TEXT("CardNoDropZone");

    // NavMesh 지면 Z 대비 이 값 이상 벗어난 위치는 비정상(나무 꼭대기 등)으로 보고 버린다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandMaxGroundZDelta = 400.0f;

    // 카드 바로 위 머리공간 검사 높이. 이 안에 장애물이 있으면 캐노피/바위 아래로 보고 제외.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Island")
    float CardIslandOverheadClearance = 90.0f;

    UPROPERTY()
    TMap<EGamePhase, TObjectPtr<UPhaseStrategy>> StrategyMap;

    UPROPERTY()
    UPhaseStrategy* CurrentStrategy;

private:
    FTimerHandle PhaseTimerHandle;
    FTimerHandle MatchEndShutdownTimerHandle;

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

    struct FCardIslandDropZone
    {
        TWeakObjectPtr<AActor> ZoneActor;
        FBox Bounds;
        FVector Center = FVector::ZeroVector;
        FName IslandKey = NAME_None;
        FString Source;
        int32 SortOrder = 1000;
    };

    int32 NextCardInstanceId = 1;
    TMap<int32, FServerCardRecord> ServerCardRecords;

    UPROPERTY()
    TArray<TObjectPtr<ACardDropActor>> ActiveCardDrops;

    enum class ESeotdaSpecialRule : uint8
    {
        None = 0,
        TtaengJabi,
        Gusa,
        MeongteongguriGusa,
        AmhaengEosa
    };

    struct FSeotdaHandResult
    {
        int32 Rank = 0;
        int32 SubRank = 0;
        FString Name;
        TArray<int32> UsedCardInstanceIds;
        ESeotdaSpecialRule SpecialRule = ESeotdaSpecialRule::None;
        bool bForcesRedeal = false;

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
    bool bSeotdaRoundResolved = false;
    FString LastSeotdaRoundResultSummary = TEXT("Pending");

    // ?쒕쾭媛 踰좏똿 ?쒖옉 ??湲곕낯?쇰줈 ?ｌ뼱二쇰뒗 ?먮룉. ?뚮젅?댁뼱 ?덉뿉?쒕뒗 李④컧?섏? ?딆쓬.
    UPROPERTY(EditDefaultsOnly, Category = "Seotda|Betting")
    int32 SeotdaServerSeedPot = 2;

    // 踰좏똿 ?쒖옉 吏곹썑 Call???꾩슂??湲곕낯 湲덉븸.
    UPROPERTY(EditDefaultsOnly, Category = "Seotda|Betting")
    int32 SeotdaBaseCallBet = 2;



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
    void ShutdownDedicatedServerAfterMatchEnd();
    void NotifyIocpMatchEnd(const FString& WinnerName, const FString& MoneySummary) const;


    void SetPlayerPawnGameplayEnabled(bool bEnabled, const TCHAR* Context);
    void ClearPlayerPawnMovementBases(const TCHAR* Context);

    void StartTimedServerPhase(EDediServerPhase NewPhase, int32 DurationSeconds);
    void OnServerPhaseTick();
    void FinishCurrentServerPhase(const TCHAR* Reason);
    void ClearServerPhaseTimer();
    void SetServerRemainingTime(int32 NewTime);

    FString GetOwnedCardsDebugString(const AMainPlayerState* PS) const;
    TArray<ECardID> BuildCardBundleIDs() const;
    void ShuffleCardIDs(TArray<ECardID>& CardIDs) const;
    FVector GetDistributedCardDropLocation(int32 Index, int32 TotalCount) const;

    TArray<TArray<ECardID>> BuildBalancedIslandCardGroups() const;
    int32 GetCardIslandBalanceValue(ECardID CardID) const;
    int32 GetCardIslandGroupBalanceValue(const TArray<ECardID>& CardIDs) const;
    TArray<FCardIslandDropZone> FindCardIslandDropZones() const;
    bool IsSeasonIslandActorName(const FString& ActorName) const;
    bool IsCardIslandSurfaceWalkable(const FHitResult& Hit) const;
    bool IsCardDropLocationClear(const FVector& CandidateLocation) const;
    bool IsFarEnoughFromIslandCards(const FVector& CandidateLocation, const TArray<FVector>& ExistingIslandLocations) const;
    bool PickIslandCardDropLocation(const FCardIslandDropZone& DropZone, const TArray<FVector>& ExistingIslandLocations, int32 IslandIndex, int32 SlotIndex, FVector& OutLocation) const;
    bool IsCardDropZSane(const FCardIslandDropZone& DropZone, float ReferenceNavZ, const FVector& Candidate) const;
    bool IsInsideNoDropZone(const FVector& Candidate) const;
    bool HasOverheadClearance(const FVector& Candidate) const;

    int32 CreateCardInstance(ECardID CardID);

public:
    ACardDropActor* SpawnCardDrop(ECardID CardID, const FVector& SpawnLocation);

private:
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
    void BroadcastSeotdaState() const;

    AMainPlayerState* GetCurrentSeotdaTurnPlayer() const;
    int32 GetSeotdaPlayerMoney(const AMainPlayerState* TargetPS) const;
    int32 PaySeotdaBet(AMainPlayerState* TargetPS, int32 Amount);
    int32 GetActiveSeotdaPlayerCount() const;
    bool AreSeotdaBetsSettled() const;
    FSeotdaHandResult EvaluateSeotdaHand(const FOwnedCardInfo& FirstCard, const FOwnedCardInfo& SecondCard) const;
    int32 GetSeotdaCardMonth(ECardID CardID) const;
    bool IsSeotdaGwang(ECardID CardID) const;
    bool HasSeotdaMonths(int32 FirstMonth, int32 SecondMonth, int32 A, int32 B) const;
    int32 CompareSeotdaHands(const FSeotdaHandResult& A, const FSeotdaHandResult& B) const;
    bool ShouldForceSeotdaRedeal() const;
    bool TryApplySeotdaRedealFromRemainingCards(const TCHAR* Reason);


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
