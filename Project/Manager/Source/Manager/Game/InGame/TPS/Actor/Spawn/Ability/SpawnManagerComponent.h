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

	UPROPERTY()
	TArray<class AA_Spawn*> CenterSpawns;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// 현재 (서버 권위) 게임모드에 붙은 SpawnManagerComponent를 찾아 반환. 없으면 nullptr.
	// 사망/리스폰/낙사/스폰선택 등 여러 곳에서 공유하던 "게임모드 → 스폰매니저" 탐색을 한 곳으로.
	static USpawnManagerComponent* GetActive(const UObject* WorldContextObject);

	void InitializeSpawnPoints();
	bool GetSpawnLocation(int32 ID, FVector& OutLocation);

	class AA_Spawn* GetUniqueRandomSpawnActor();
	int32 GetAvailableSpawnCount() const;

	int32 GetRandomSpawnID() const;
	class AA_Spawn* GetRandomCenterSpawnActor();

	// AvailableSpawns를 제자리에서 섞음(원소 제거 없음). 매 라운드 재배치처럼 반복 호출해도 풀이 고갈되지 않는다.
	void ShuffleAvailableSpawns();
	const TArray<class AA_Spawn*>& GetAvailableSpawnsView() const { return AvailableSpawns; }

private:
	TMap<int32, class AA_Spawn*> SpawnPointMap;
		
};
