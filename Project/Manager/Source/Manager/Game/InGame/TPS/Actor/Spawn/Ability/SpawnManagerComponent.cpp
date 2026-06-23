// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Kismet/GameplayStatics.h"

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
	SpawnPointMap.Empty();
	AvailableSpawns.Empty();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AA_Spawn::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors) {
		AA_Spawn* SP = Cast<AA_Spawn>(Actor);
		if (SP) {
			if (SpawnPointMap.Contains(SP->SpawnPointID))
				continue;
			SpawnPointMap.Add(SP->SpawnPointID,SP);
			AvailableSpawns.Add(SP);
		}
	}
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