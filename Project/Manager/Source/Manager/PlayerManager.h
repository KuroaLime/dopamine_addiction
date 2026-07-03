// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameFramework/Character.h"
#include "PlayerManager.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRegistered, ACharacter*, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerUnregistered, ACharacter*, Player);

UCLASS()
class MANAGER_API UPlayerManager : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	
	virtual TStatId GetStatId() const override;

	virtual bool IsTickable() const override { return true; }

	void RequestRegister(ACharacter* NewPlayer);
	void RequestUnregister(ACharacter* OldPlayer);

	TArray<ACharacter*> GetAllPlayers() const;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerRegistered OnPlayerRegistered;
	UPROPERTY(BlueprintAssignable)
	FOnPlayerUnregistered OnPlayerUnregistered;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<ACharacter>> ManagedPlayers;

	UPROPERTY()
	TArray<TWeakObjectPtr<ACharacter>> PendingAdd;
	UPROPERTY()
	TArray<TWeakObjectPtr<ACharacter>> PendingRemove;

private:
	void ApplyPending();
	void CleanupInvalid();
};
