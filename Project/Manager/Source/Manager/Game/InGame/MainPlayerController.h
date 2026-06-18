// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
#include "MainPlayerController.generated.h"

class UInputHandler;
class UUIHandler;
/**
 * 
 */
UCLASS()
class MANAGER_API AMainPlayerController : public APlayerController,
										  public IPhasePlayerControllerInterface
{
	GENERATED_BODY()
	
public:
	virtual void SwitchMode(EGamePhase NewPhase) override;
	virtual void SwitchToLevel(FName LevelToUnload, FName LevelToLoad) override;
	virtual void PushMode(EGamePhase NewPhase) override;
	virtual void PopMode() override;
	virtual EGamePhase GetCurrentPhase() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UFUNCTION(Server, Reliable)
	void Server_PushMode(EGamePhase NewPhase);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PushMode(EGamePhase NewPhase);

	UFUNCTION(Server, Reliable)
	void Server_PopMode();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PopMode();


	//랜덤카드3장 생성하기위해서
	//버튼 눌렀을 때 클라가 호출
	//3개의 카드 데이터 받아오기
	UFUNCTION(Server, Reliable)
	void Server_RequestRandomUpgradeOptions();

	//서버가 랜덤을 돌리고 클라UI에 줄때 사용
	UFUNCTION(Client, Reliable)
	void Client_ReceiveRandomUpgradeOptions(const TArray<EUpgradeType>&Options);

	//UI에서 카드를 골랐ㅇ르 때 클라가 호출
	UFUNCTION(Server, Reliable)
	void Server_SelectUpgradeOption(int32 SelectedIndex);

	//서버 검증용
	UPROPERTY()
	TArray<EUpgradeType> CurrentUpgradeOptions;
};
