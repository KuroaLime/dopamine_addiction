#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/InGame/Interface/PhaseGameModeInterface.h"
#include "MainGameMode.generated.h"

class UPhaseStrategy;
class APlayerController;
class AController;

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
    int32 DebugBattleRoyaleDuration = 30;

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

    UPROPERTY()
    TMap<EGamePhase, TObjectPtr<UPhaseStrategy>> StrategyMap;

    UPROPERTY()
    UPhaseStrategy* CurrentStrategy;

private:
    FTimerHandle PhaseTimerHandle;
    EDediServerPhase CurrentServerPhase = EDediServerPhase::None;
    bool bGameEndReached = false;

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

    int32 GetReadyDuration() const;
    int32 GetBattleRoyaleDuration() const;
    int32 GetTransitionDuration() const;
    int32 GetCardGameDuration() const;
    int32 GetResultDuration() const;
    const TCHAR* GetServerPhaseName(EDediServerPhase Phase) const;
};
