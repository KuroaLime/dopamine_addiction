// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Game/Protocol_Client/Protocol_D.h"
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
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Lobby")
	TSubclassOf<AActor> CharacterClass;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedCharacter;

	UPROPERTY(VisibleAnywhere, Category = "Lobby")
	TObjectPtr<UArrowComponent> SpawnArrow;

	UPROPERTY(VisibleAnywhere, Category = "UserInfoUI")
	class UWidgetComponent* UserInfoWidget;
protected:
	UPROPERTY(EditAnywhere, Category = "Lobby")
	TArray<USkeletalMesh*> LobbyMeshes;
private:

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SpawnCharacter();
	void ClearCharacter();

	void UpdateUserInfoWidget(const FRoomMemberInfoView& MemberInfo);
	void ClearUserInfoWidget();

	UFUNCTION()
	void UpdateLobbyCharacters(const TArray<FRoomMemberInfoView>& Members);
};
