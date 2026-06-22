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

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);

	UFUNCTION(Client, Reliable)
	void Client_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);

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
	void Server_RequestPickupNearestCard();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitSeotdaSelection(bool bCard0, bool bCard1, bool bCard2);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSeotdaBetAction(EBettingAction Action);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_TPSFireFromClient(FVector ViewLocation, FRotator ViewRotation);

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
	// 현재 레벨을 조회하는 헬퍼
	UFUNCTION()
	int32 GetCurrentUpgradeLevel(class AMainPlayerState* PS, EUpgradeType Type);

	UPROPERTY()
	TArray<FRandomCardOption> CurrentUpgradeOptions;
	
	UFUNCTION(Server, Reliable)
	void Server_SelectStaticUpgradeOption(int32 SelectedIndex);

	UFUNCTION(Server, Reliable)
	void Server_SetUITimer(int32 time);

	UFUNCTION(Client, Reliable)
	void Client_SetUITimer(int32 time);

	
};
