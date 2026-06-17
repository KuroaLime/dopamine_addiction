// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinRegulator.generated.h"

UCLASS()
class MANAGER_API ACoinRegulator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACoinRegulator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Body;

	UPROPERTY(VisibleAnywhere)
	int32 ID;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gold")
	class UCharacterStateComponent* TargetPS = nullptr;
public:
	UPROPERTY(VisibleAnywhere, Category = UI)
	class UWidgetComponent* HavingCoin;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> CoinWidgetClass;
};
