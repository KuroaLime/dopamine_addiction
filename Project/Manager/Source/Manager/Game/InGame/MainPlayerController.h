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

	EGamePhase CurrentPhase;

protected:
	UPROPERTY()
	TMap<EGamePhase, TObjectPtr<UInputHandler>> InputHandlerMap;

	UPROPERTY()
	TMap<EGamePhase, TObjectPtr<UUIHandler>> UIHandlerMap;

private:
	void InitHandler();
	void SetupHandlerInput();

public:
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);

	UFUNCTION(Client, Reliable)
	void Client_SwitchToLevel(FName LevelToUnload, FName LevelToLoad);
};
