#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/InGame/Interface/PhaseGameModeInterface.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "Game/InGame/Card/CardPlacementService.h"
#include "MainGameMode.generated.h"

class UPhaseStrategy;
class APlayerController;
class AController;
class AMainPlayerController;
class AMainPlayerState;
class ACardDropActor;
class USpawnManagerComponent;
class UCardGameService;

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

    virtual FString InitNewPlayer(
        APlayerController* NewPlayerController,
        const FUniqueNetIdRepl& UniqueId,
        const FString& Options,
        const FString& Portal) override;

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
    // ===== Phase Strategy Context API =====
    // 페이즈 전략(UTPSPhaseStrategy/UCardPhaseStrategy)이 셋업을 수행할 때 호출하는 공개 연산.
    // 상태/타이머는 GameMode가 소유하며, 전략은 friend 없이 이 API로만 Context를 조작한다.
    void EnsureBattleRoyaleStageLoaded();
    void HandleClientStreamLevelLoaded(AMainPlayerController* PlayerController, FName LoadedLevel, EGamePhase ClientPhase);
    void RequestBattleRoyaleCardSpawnAfterStreamReady(const TCHAR* Context);
    void SetPlayerPawnGameplayEnabled(bool bEnabled, const TCHAR* Context);
    void SetPlayerPawnGameplayState(bool bVisible, bool bMovementEnabled, bool bCollisionEnabled, const TCHAR* Context);
    void ClearPlayerPawnMovementBases(const TCHAR* Context);
    void RequestMovePlayersToCardIslandSeats(const TCHAR* Context);

public:
    // 카드게임 도메인(카드/섯다 상태·로직)은 UCardGameService(CardGameService.h)가 소유.
    // 외부(PC RPC)·전략은 이 접근자를 통해 서비스에 위임한다.
    UCardGameService* GetCardGameService() const { return CardGameService; }

    // 서비스가 참조하는 매치 상태/설정 접근자(서비스 Init/런타임에서 사용).
    int32 GetCurrentRound() const { return CurrentRound; }
    EDediServerPhase GetCurrentServerPhase() const { return CurrentServerPhase; }
    float GetCardPickupRange() const { return CardPickupRange; }
    int32 GetMaxCardsPerPlayerPerRound() const { return MaxCardsPerPlayerPerRound; }
    TSubclassOf<ACardDropActor> GetCardDropActorClass() const { return CardDropActorClass; }
    int32 GetSeotdaServerSeedPot() const { return SeotdaServerSeedPot; }
    int32 GetSeotdaBaseCallBet() const { return SeotdaBaseCallBet; }

public:
    bool IsBattleRoyalePhase() const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Setup")
    TMap<EGamePhase, TSubclassOf<UPhaseStrategy>> StrategyClassMap;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Setup")
    EGamePhase InitialPhase = EGamePhase::TPS;

    UPROPERTY(BlueprintReadOnly, Category = "Dedicated Server")
    int32 DediRoomId = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dedicated Server")
    int32 RequiredPlayerCount = 1;

    TSet<int64> AllowedDediTickets;
    TSet<int64> UsedDediTickets;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    FName CardPlayerSeatTag = TEXT("CardPlayerSeat");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    FVector CardPlayerFallbackCenter = FVector(0.0f, 0.0f, 300.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    bool bUseCardPlayerFallbackSeats = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    float CardPlayerSeatSpacing = 240.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    float CardPlayerSeatZOffset = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    int32 CardPlayerSeatMoveMaxRetries = 20;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|CardTransition")
    float CardPlayerSeatMoveRetryInterval = 0.25f;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Server")
    float BattleRoyaleCardSpawnGateRetryInterval = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Server")
    int32 BattleRoyaleCardSpawnGateMaxRetries = 0;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    int32 CardDeathDropMaxAttemptsPerCard = 32;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropStartRadius = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropRadiusStep = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropMaxRadius = 520.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropMinCardDistance = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropGroundOffsetZ = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    FVector CardDeathDropNavProjectExtent = FVector(180.0f, 180.0f, 500.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|DeathDrop")
    float CardDeathDropMaxNavProjectDistance = 180.0f;

    UPROPERTY()
    TMap<EGamePhase, TObjectPtr<UPhaseStrategy>> StrategyMap;

    UPROPERTY()
    UPhaseStrategy* CurrentStrategy;

private:
    FTimerHandle PhaseTimerHandle;
    FTimerHandle MatchEndShutdownTimerHandle;
    FTimerHandle CardSeatMoveRetryTimerHandle;
    FTimerHandle BattleRoyaleCardSpawnGateTimerHandle;
    int32 ServerStreamingLatentActionId = 10000;
    int32 CardSeatMoveRetryCount = 0;
    int32 BattleRoyaleCardSpawnGateRetryCount = 0;
    FString PendingCardSeatMoveContext;
    FString PendingBattleRoyaleCardSpawnContext;
    TMap<TWeakObjectPtr<AMainPlayerController>, FName> ClientLoadedStreamLevels;
    TMap<TWeakObjectPtr<AMainPlayerController>, EGamePhase> ClientLoadedStreamPhases;
    FName PendingBattleRoyaleCardSpawnLevel = NAME_None;
    int32 PendingBattleRoyaleCardSpawnRound = 0;
    bool bPendingBattleRoyaleCardSpawn = false;
    bool bBattleRoyaleCardsSpawnedThisPhase = false;

    EDediServerPhase CurrentServerPhase = EDediServerPhase::None;
    bool bGameEndReached = false;

    // 카드/섯다 런타임 상태(ServerCardRecords/SeotdaRoundStates 등)와 구조체는
    // UCardGameService(CardGameService.h)로 이전됨. GameMode는 서비스 포인터만 보유한다.
    UPROPERTY()
    UCardGameService* CardGameService = nullptr;

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
    void NotifyIocpServerReady() const;
    void NotifyIocpMatchEnd(const FString& WinnerName, const FString& MoneySummary) const;
    void LoadServerStreamLevelForPhase(FName LevelToLoad, const TCHAR* Context);
    void ResetClientStreamLevelAcks(const TCHAR* Context);
    bool HaveRequiredClientsLoadedStreamLevel(FName TargetLevel, int32& OutLoadedClients, int32& OutTargetClients) const;
    void TrySpawnBattleRoyaleCardsWhenStreamReady();
    void ScheduleBattleRoyaleCardSpawnGateRetry(const TCHAR* Context);
    void RetryBattleRoyaleCardSpawnGate();
    void ClearBattleRoyaleCardSpawnGate(const TCHAR* Context);


    void ScheduleCardSeatMoveRetry(const TCHAR* Context);
    void RetryMovePlayersToCardIslandSeats();
    bool MovePlayersToCardIslandSeats(const TCHAR* Context);
    TArray<FTransform> BuildCardPlayerSeatTransforms(int32 RequiredCount) const;

    void StartTimedServerPhase(EDediServerPhase NewPhase, int32 DurationSeconds);
    void OnServerPhaseTick();
public:
    // UCardGameService가 OwnerGM을 통해 호출하는 GameMode 연산(서버 페이즈/카드 배치/설정).
    void SetServerRemainingTime(int32 NewTime);
    void SetRemainingPhaseSeconds(int32 NewValue) { RemainingPhaseSeconds = NewValue; }
    void FinishCurrentServerPhase(const TCHAR* Reason);
    void ClearServerPhaseTimer();
    FCardPlacementService MakeCardPlacementService() const;
    const TCHAR* GetServerPhaseName(EDediServerPhase Phase) const;
    float GetCardDeathDropStartRadius() const { return CardDeathDropStartRadius; }
    float GetCardDeathDropGroundOffsetZ() const { return CardDeathDropGroundOffsetZ; }
    int32 GetCardIslandDropExpectedZoneCount() const { return CardIslandDropExpectedZoneCount; }

private:
    // 카드/섯다 진입점·로직(TryPickupCard/SubmitSeotda*/ResolveSeotdaRoundResult 등)은
    // UCardGameService(CardGameService.h)로 이전됨.


    int32 GetReadyDuration() const;
    int32 GetBattleRoyaleDuration() const;
    int32 GetTransitionDuration() const;
    int32 GetCardGameDuration() const;
    int32 GetResultDuration() const;

    public:
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameMode|Spawn")
        USpawnManagerComponent* SpawnManager;
        virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
        virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};
