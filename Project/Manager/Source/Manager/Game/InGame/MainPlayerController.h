// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

#include "MainPlayerController.generated.h"
class UInputHandler;
class UUIHandler;
class ACardDropActor;

UCLASS()
class MANAGER_API AMainPlayerController : public APlayerController,
										  public IPhasePlayerControllerInterface
{
	GENERATED_BODY()
	
public:
	virtual void SwitchMode(EGamePhase NewPhase) override;
	virtual void SwitchToLevel(FName LevelToUnload, FName LevelToLoad) override;
	virtual void SwitchState(EGamePhase NewPhase) override;
	virtual void PushMode(EGamePhase NewPhase) override;
	virtual void PopMode() override;
	virtual void SetUITimer(int32 time) override;
	virtual EGamePhase GetCurrentPhase() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

public:
	virtual void SetupInputComponent() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Controller|Setup")
	TMap<EGamePhase, TSubclassOf<UInputHandler>> InputHandlerClassMap;

	UPROPERTY(EditDefaultsOnly, Category = "Controller|Setup")
	TMap<EGamePhase, TSubclassOf<UUIHandler>> UIHandlerClassMap;

	UPROPERTY(BlueprintReadOnly, Category = "Controller|State")
	EGamePhase CurrentPhase = EGamePhase::TPS;

protected:
	UPROPERTY()
	TMap<EGamePhase, TObjectPtr<UInputHandler>> InputHandlerMap;

	UPROPERTY()
	TMap<EGamePhase, TObjectPtr<UUIHandler>> UIHandlerMap;

	TArray<EGamePhase> PhaseStack;

private:
	void InitHandler();
	void SetupHandlerInput();

	
public:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SwitchMode(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable)
	void Server_SwitchMode(EGamePhase NewPhase);

	void ApplySwitchMode(EGamePhase NewPhase);
	void SetGameplayInputLocked(bool bLocked, const TCHAR* Context);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);

	UFUNCTION(Client, Reliable)
	void Client_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);

	UFUNCTION(Client, Reliable)
	void Client_SynchronizePhase(EGamePhase ServerPhase);

	UFUNCTION()
	void OnClientStreamLevelLoaded();

	UFUNCTION()
	void OnClientStreamLevelUnloaded();

	bool TryReportPendingClientLevelReady(const TCHAR* Context);
	void SchedulePendingClientLevelReadinessRetry();
	void RetryPendingClientLevelReadiness();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ReportStreamLevelLoaded(FName LoadedLevel, EGamePhase ClientPhase);

	UFUNCTION(Client, Reliable)
	void Client_ExpectCardBundle(int32 Round, int32 BundleGeneration, const TArray<int32>& ExpectedInstanceIds);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ReportCardBundleReady(int32 Round, int32 BundleGeneration, int32 VisibleCount);

	UFUNCTION(Client, Reliable)
	void Client_SetGameplayInputLocked(bool bLocked, const FString& Context);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchState(EGamePhase NewPhase);

	UFUNCTION(Client, Reliable)
	void Client_SwitchState(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable)
	void Server_PushMode(EGamePhase NewPhase);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PushMode(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable)
	void Server_PopMode();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PopMode();

	/*UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestUpgrade(int32 ItemID);*/

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestPickupCard(ACardDropActor* TargetCard);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitSeotdaSelection(bool bCard0, bool bCard1, bool bCard2);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSeotdaBetAction(EBettingAction Action);

	UFUNCTION(Server, Reliable)
	void Server_RequestRandomUpgradeOptions();

	UFUNCTION(Client, Reliable)
	void Client_ReceiveRandomUpgradeOptions(const TArray<FRandomCardOption>& Options);

	UFUNCTION(Server, Reliable)
	void Server_SelectUpgradeOption(int32 SelectedIndex);

	UFUNCTION()
	EUpgradeType GetStaticUpgradeTypeFromIndex(int32 Index);
	UFUNCTION()
	int32 GetStaticUpgradeCost(EUpgradeType Type, int32 CurrentLevel);

	UFUNCTION()
	int32 GetCurrentUpgradeLevel(class AMainPlayerState* PS, EUpgradeType Type);

	UPROPERTY()
	TArray<FRandomCardOption> CurrentUpgradeOptions;

	UPROPERTY()
	FName PendingClientStreamLevelToLoad = NAME_None;

	UPROPERTY()
	FName PendingClientStreamLevelToUnload = NAME_None;

	FTimerHandle PendingClientLevelReadinessTimerHandle;
	int32 ClientStreamingLatentActionId = 1000;
	int32 PendingClientLevelReadinessRetryCount = 0;
	bool bPendingClientLevelReadyReported = false;

	bool TryReportPendingCardBundleReady(const TCHAR* Context);
	void SchedulePendingCardBundleReadinessRetry();
	void RetryPendingCardBundleReadiness();
	void ClearPendingCardBundleExpectation();

	FTimerHandle PendingClientCardBundleReadinessTimerHandle;
	int32 PendingClientCardBundleRound = 0;
	int32 PendingClientCardBundleGeneration = 0;
	int32 PendingClientCardBundleRetryCount = 0;
	bool bPendingClientCardBundleReadyReported = false;

	UPROPERTY()
	TArray<int32> PendingClientExpectedCardInstanceIds;

	UPROPERTY()
	bool bGameplayInputLocked = false;

	void ApplyGameplayInputLock(bool bLocked, const TCHAR* Context);
	
	UFUNCTION(Server, Reliable)
	void Server_SelectStaticUpgradeOption(int32 SelectedIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestDiscardCard(int32 CardInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_SetUITimer(int32 time);

	UFUNCTION(Client, Reliable)
	void Client_SetUITimer(int32 time);


	UFUNCTION(Client, Reliable)
	void Client_ShowSeotdaResult(const FString& ResultText);

    UFUNCTION(BlueprintCallable, Category = "Seotda")
    void ReturnToLobbyFromMatchEnd();

UFUNCTION(Client, Reliable)
void Client_UpdateSeotdaState(
int32 Round,
bool bBettingActive,
const FString& CurrentTurnPlayerName,
int32 Pot,
int32 CurrentBet,
int32 MyBetMoney,
int32 NeedCall,
bool bMyTurn,
bool bMySubmitted,
bool bMyFolded,
bool bRoundResolved
);

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiRound = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiBettingActive = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
FString SeotdaUiCurrentTurnPlayerName;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiPot = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiCurrentBet = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiMyBetMoney = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiNeedCall = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiMyTurn = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiMySubmitted = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiMyFolded = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiRoundResolved = false;

    UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
    bool bSeotdaUiMatchEnded = false;

    UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
    FString SeotdaUiLastResultText;
};
