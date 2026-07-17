// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Algo/RandomShuffle.h"

USpawnManagerComponent* USpawnManagerComponent::GetActive(const UObject* WorldContextObject)
{
	// 게임모드는 서버 권위에만 존재 → 이 헬퍼는 서버 경로(사망/리스폰/낙사/스폰선택)에서 호출됨.
	AGameModeBase* GM = UGameplayStatics::GetGameMode(WorldContextObject);
	return GM ? GM->FindComponentByClass<USpawnManagerComponent>() : nullptr;
}

// Sets default values for this component's properties
USpawnManagerComponent::USpawnManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void USpawnManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeSpawnPoints();
	// ...
	
}


// Called every frame
void USpawnManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
//가지고 있는 애들 초기화 및 채우기
void USpawnManagerComponent::InitializeSpawnPoints() {
	UWorld* World = GetWorld();
	if (!World) {
		UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] InitializeSpawnPoints (Owner: %s): World is NULL! Skipping initialization to preserve existing spawns."), *GetOwner()->GetName());
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AA_Spawn::StaticClass(), FoundActors);

	if (FoundActors.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] InitializeSpawnPoints (Owner: %s): Found 0 AA_Spawn actors. Keeping existing spawn points."), *GetOwner()->GetName());
		return;
	}

	SpawnPointMap.Empty();
	AvailableSpawns.Empty();
	CenterSpawns.Empty();

	for (AActor* Actor : FoundActors) {
		AA_Spawn* SP = Cast<AA_Spawn>(Actor);
		if (SP) {
			if (SpawnPointMap.Contains(SP->SpawnPointID)) {
				UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] InitializeSpawnPoints (Owner: %s): DUPLICATE SpawnPointID = %d on '%s' (Already mapped to '%s'). Keeping in spawn list but skipping from lookup map."),
					*GetOwner()->GetName(), SP->SpawnPointID, *SP->GetName(), *SpawnPointMap[SP->SpawnPointID]->GetName());
			} else {
				SpawnPointMap.Add(SP->SpawnPointID, SP);
			}
			
			if (SP->ActorHasTag(TEXT("CenterSpawner"))) {
				CenterSpawns.Add(SP);
			} else {
				AvailableSpawns.Add(SP);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] InitializeSpawnPoints (Owner: %s): Total Found Spawns = %d, Center Spawns = %d, Available Spawns = %d"),
		*GetOwner()->GetName(), FoundActors.Num(), CenterSpawns.Num(), AvailableSpawns.Num());
}
//아이디를 받아서 위치를 얻어내는 함수
bool USpawnManagerComponent::GetSpawnLocation(int32 ID, FVector& OutLocation) {
	AA_Spawn** FoundPoint = SpawnPointMap.Find(ID);
	if (FoundPoint && *FoundPoint) {
		OutLocation = (*FoundPoint)->GetActorLocation();
		return true;
	}
	return false;
}
//랜덤하게 아이디를 받아가는 함수
int32 USpawnManagerComponent::GetRandomSpawnID() const{
	if (SpawnPointMap.Num() == 0)
		return -1;

	TArray<int32> Keys;
	SpawnPointMap.GetKeys(Keys);
	int32 RandomIndex = FMath::RandRange(0, Keys.Num() - 1);

	return Keys[RandomIndex];
}

AA_Spawn* USpawnManagerComponent::GetUniqueRandomSpawnActor() {
	if (AvailableSpawns.Num() == 0) return nullptr;

	int32 RandomIndex = FMath::RandRange(0, AvailableSpawns.Num() - 1);
	AA_Spawn* SelectedSpawn = AvailableSpawns[RandomIndex];

	AvailableSpawns.RemoveAt(RandomIndex);

	return SelectedSpawn;
}
int32 USpawnManagerComponent::GetAvailableSpawnCount() const {
	return AvailableSpawns.Num();
}

void USpawnManagerComponent::ShuffleAvailableSpawns() {
	Algo::RandomShuffle(AvailableSpawns);
}

AA_Spawn* USpawnManagerComponent::GetRandomCenterSpawnActor() {
	if (CenterSpawns.Num() == 0) {
		UE_LOG(LogTemp, Error, TEXT("[SpawnManager] GetRandomCenterSpawnActor (Owner: %s): CenterSpawns is EMPTY! Falling back to any spawner."), *GetOwner()->GetName());
		if (SpawnPointMap.Num() == 0) return nullptr;
		TArray<int32> Keys;
		SpawnPointMap.GetKeys(Keys);
		int32 RandomIndex = FMath::RandRange(0, Keys.Num() - 1);
		return SpawnPointMap[Keys[RandomIndex]];
	}

	int32 RandomIndex = FMath::RandRange(0, CenterSpawns.Num() - 1);
	return CenterSpawns[RandomIndex];
}