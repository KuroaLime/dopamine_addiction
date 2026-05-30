// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TPSGameMode.generated.h"

/**
 * 
 */

class ATPSPlayerController;
class ATPSGameState;
class USpawnManagerComponent;
class APlayerController;
class AController;

UCLASS(Abstract)
class MANAGER_API ATPSGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ATPSGameMode();

	virtual void BeginPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Manager")
	USpawnManagerComponent* SpawnManager;

	void RoundTimerTick();
	void OnPlayerAction(AActor* Executor, FName ActionName);

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:
	ATPSGameState* pTGS;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	int32 BattleRoyaleDuration = 60;

	UPROPERTY(BlueprintReadOnly, Category = "Dedicated Server")
	bool bGameStarted = false;
	
	FORCEINLINE ATPSGameState* GetGS() const;

	void StartBattleRoyalePhase();

private:
	FTimerHandle RoundTimerHandle;
};
