// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpawnManagerComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API USpawnManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USpawnManagerComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY()
	TArray<class AA_Spawn*> AvailableSpawns;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void InitializeSpawnPoints();
	bool GetSpawnLocation(int32 ID, FVector& OutLocation);
	
	class AA_Spawn* GetUniqueRandomSpawnActor();
	int32 GetAvailableSpawnCount() const;
	
	int32 GetRandomSpawnID() const;

private:
	TMap<int32, class AA_Spawn*> SpawnPointMap;
		
};
