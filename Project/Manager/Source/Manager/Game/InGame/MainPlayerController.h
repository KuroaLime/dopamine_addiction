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
	struct FServerRpcRateLimitState
	{
		double LastAcceptedSeconds = 0.0;
		double RejectedWindowStartSeconds = 0.0;
		double LastWarningSeconds = 0.0;
		int32 RejectedInWindow = 0;
		bool bHasAcceptedRequest = false;
	};

	void InitHandler();
	void SetupHandlerInput();
	bool TryConsumeServerRpcRateLimit(
		FServerRpcRateLimitState& State,
		double MinimumIntervalSeconds,
		const TCHAR* RpcName);
	void SendPendingServerPositionCorrection(const TCHAR* Reason);
	void RetryPendingServerPositionCorrection();
	void ClearPendingServerPositionCorrection();

	FServerRpcRateLimitState CardPickupRateLimitState;
	FServerRpcRateLimitState CardDiscardRateLimitState;
	FServerRpcRateLimitState SeotdaRevealRateLimitState;
	FServerRpcRateLimitState SeotdaSelectionRateLimitState;
	FServerRpcRateLimitState SeotdaBetRateLimitState;
	FServerRpcRateLimitState StreamLevelAckRateLimitState;
	FServerRpcRateLimitState CardBundleAckRateLimitState;
	FServerRpcRateLimitState ShopPushRateLimitState;
	FServerRpcRateLimitState ShopPopRateLimitState;
	FServerRpcRateLimitState ShopRandomRollRateLimitState;
	FServerRpcRateLimitState ShopRandomSelectionRateLimitState;
	FServerRpcRateLimitState ShopStaticUpgradeRateLimitState;
	FServerRpcRateLimitState ShopWeaponUpgradeRateLimitState;
	FTimerHandle ServerPositionCorrectionRetryTimerHandle;
	FVector PendingServerPositionCorrectionLocation = FVector::ZeroVector;
	FRotator PendingServerPositionCorrectionRotation = FRotator::ZeroRotator;
	FString PendingServerPositionCorrectionContext;
	int32 PendingServerPositionCorrectionSendCount = 0;
	bool bPendingServerPositionCorrection = false;

	
public:
	// Shared by the direct pickup RPC and Ability.Action.PickUp.
	bool TryConsumeCardPickupRequest();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SwitchMode(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchMode(EGamePhase NewPhase);

	void ApplySwitchMode(EGamePhase NewPhase);
	// 카드 라운드(밀폐된 한옥방) 동안 메인 월드(퍼시스턴트 레벨)의 야외 DirectionalLight/SkyLight가
	// 겹쳐 비치는 걸 막기 위해 껐다 켠다. Card_Game_Stage 서브레벨 자체 조명은 건드리지 않는다.
	void UpdateEnvironmentLightsForPhase(EGamePhase NewPhase);
	void SetGameplayInputLocked(bool bLocked, const TCHAR* Context);
	void StartServerAuthoritativePositionCorrection(
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		const TCHAR* Context);
	bool ForceCloseShopMode(const TCHAR* Context);

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

	UFUNCTION(Client, Reliable)
	void Client_ForceCloseShopMode(const FString& Context);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchState(EGamePhase NewPhase);

	UFUNCTION(Client, Reliable)
	void Client_SwitchState(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable, WithValidation)
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
	void Server_RevealSeotdaCard(bool bCard0, bool bCard1, bool bCard2);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveSeotdaRevealResult(bool bAccepted, const FString& Reason);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitSeotdaSelection(bool bCard0, bool bCard1, bool bCard2);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveSeotdaSelectionResult(bool bAccepted, const FString& Reason);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSeotdaBetAction(EBettingAction Action);

	UFUNCTION(Server, Reliable)
	void Server_RequestRandomUpgradeOptions();

	UFUNCTION(Client, Reliable)
	void Client_ReceiveRandomUpgradeOptions(const TArray<FRandomCardOption>& Options);

	UFUNCTION(Server, Reliable, WithValidation)
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
	bool ApplyForceCloseShopMode(const TCHAR* Context);
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SelectStaticUpgradeOption(int32 SelectedIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PurchaseWeaponUpgrade(int32 SlotIndex);

	int32 GetWeaponUpgradePurchaseCost() const;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestDiscardCard(int32 CardInstanceId);

	// 서버가 이 클라이언트의 사격이 플레이어에게 명중했음을 확인해줄 때 호출.
	UFUNCTION(Client, Reliable)
	void Client_NotifyHitConfirmed();

	UFUNCTION(Server, Reliable, WithValidation)
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
bool bMyRevealConfirmed,
bool bMySubmitted,
bool bMyFolded,
bool bRoundResolved,
const TArray<FSeotdaOpponentInfo>& Opponents
);

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
TArray<FSeotdaOpponentInfo> SeotdaUiOpponents;

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
bool bSeotdaUiMyRevealConfirmed = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiRevealResultSerial = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiRevealAccepted = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
FString SeotdaUiRevealResultReason;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiMySubmitted = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
int32 SeotdaUiSelectionResultSerial = 0;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiSelectionAccepted = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
FString SeotdaUiSelectionResultReason;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiMyFolded = false;

UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
bool bSeotdaUiRoundResolved = false;

    UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
    bool bSeotdaUiMatchEnded = false;

    UPROPERTY(BlueprintReadOnly, Category = "Seotda UI")
    FString SeotdaUiLastResultText;
};
