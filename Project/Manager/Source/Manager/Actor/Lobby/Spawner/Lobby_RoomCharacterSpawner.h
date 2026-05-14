// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/Spawn/A_Spawn.h"
#include "Game/Lobby/GS_Lobby.h"
#include "Components/ArrowComponent.h"
#include "Lobby_RoomCharacterSpawner.generated.h"

UCLASS()
class MANAGER_API ALobby_RoomCharacterSpawner : public AA_Spawn
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALobby_RoomCharacterSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 스폰할 캐릭터 클래스 (에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "Lobby")
	TSubclassOf<AActor> CharacterClass;

	// 현재 스폰된 캐릭터를 추적
	UPROPERTY()
	TObjectPtr<AActor> SpawnedCharacter;

	UPROPERTY(VisibleAnywhere, Category = "Lobby")
	TObjectPtr<UArrowComponent> SpawnArrow;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void UpdateDisplay();

	void SpawnCharacter();

	void ClearCharacter();

};
