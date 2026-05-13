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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "UI")
	TMap<ELobbyState, TSubclassOf<UUserWidget>> LobbyWidgetClass;
	UPROPERTY()
	TMap<ELobbyState, UUserWidget*> WidgetInstances;

	//UPROPERTY()
	//class UUserWidget* LobbyWidget;
	UPROPERTY()
	class UUserWidget* CurrentWidget;
public:
	void ToggleLobbyUI(bool bSucceed, ELobbyState NewState);
};
