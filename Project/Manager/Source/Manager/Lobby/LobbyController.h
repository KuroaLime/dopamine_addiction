// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LobbyController.generated.h"



/**
 * 
 */
UENUM(BlueprintType)
enum class ELobbyState : uint8 {
	RoomList,
	InRoom,
	Settings
};

UCLASS()
class MANAGER_API ALobbyController : public APlayerController
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TMap<ELobbyState, UUserWidget*> WidgetInstances;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "UI")
	TMap<ELobbyState, TSubclassOf<UUserWidget>> LobbyWidgetClass;

	UPROPERTY()
	class UUserWidget* CurrentWidget;

	void SwitchToRoomUI(bool bIsInsideRoom);

	UPROPERTY(EditAnywhere, Category = "Lobby|UI")
	TSubclassOf<UUserWidget> RoomWidgetClass;

	UPROPERTY()
	UUserWidget* CurrentRoomWidget;

protected:
	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	FVector LobbyLocation;

	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	FVector RoomLocation;

	FTimerHandle MovementTimerHandle;

	FVector StartLocation;
	FVector TargetLocation;
	float InterpAlpha;

	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	float TravelDuration = 1.5f; 

	void ProcessInterpolatedMovement();
public:
	void ToggleLobbyUI(bool bSucceed, ELobbyState NewState);
	void MoveLobbyCamera(FVector NewTarget);

	void JoinRoomSelected(FString RoomName);
	void LeaveRoom();

	void CreateRoom(const FString& RoomName);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CreateRoom(const FString& RoomName);
};
