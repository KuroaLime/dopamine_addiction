// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Protocol_Client/Protocol_D.h"
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

	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	FVector LobbyLocation;

	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	FVector RoomLocation;

	bool bReady = false;

	////////////////////////////////////////////////////////////////
	// 자연스러운 방 이동을 위한 타이머 핸들러와 보간 변수들
	FTimerHandle MovementTimerHandle;
	FVector StartLocation;
	FVector TargetLocation;
	float InterpAlpha;

	UPROPERTY(EditAnywhere, Category = "Lobby|Setup")
	float TravelDuration = 1.5f; 
	////////////////////////////////////////////////////////////////

	void ProcessInterpolatedMovement();
public:
	void ToggleLobbyUI(bool bSucceed, ELobbyState NewState);
	void MoveLobbyCamera(FVector NewTarget);
	void TransitionToLobbyState(bool bSucceed, ELobbyState LobbyState);

	void JoinRoomSelected(uint32_t RoomId);
	void LeaveRoom();
	void StartRoom();

	void UpdateRoom();
	void CreateRoom(const FString& RoomName);
	void Client_GetRoomList(TArray<RoomInfoView> RoomList);

	void ToggleReadyState();
};
