// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyRoomWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API ULobbyRoomWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	class UImage* RoomImage;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* RoomNameText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PlayerCountText;

	UPROPERTY(meta = (BindWidget))
	class UButton* EntryButton;

public:
	void UpdateRoomInfo(const FString& Name, int32 CurrentPlayers, int32 MaxPlayers, class UTexture2D* Image);
};
