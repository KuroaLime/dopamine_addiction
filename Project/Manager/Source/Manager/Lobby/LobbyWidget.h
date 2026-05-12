// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	TArray<class ULobbyRoomWidget*> RoomButtons;

	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_0;
	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_1;
	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_2;
	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_3;
	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_4;
	UPROPERTY(meta = (BindWidget))
	class ULobbyRoomWidget* RoomButton_5;

	UPROPERTY(meta = (BindWidget))
	class UButton* PrevPageButton;
	UPROPERTY(meta = (BindWidget))
	class UButton* NextPageButton;
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PageText;

	UPROPERTY(meta = (BindWidget))
	class UButton* UpdateRoomButton;

	virtual void NativeConstruct() override;

private:
	int32 CurrentPage = 0;
	int32 MaxPage = 0;

	TArray<FString> TotalRoomNames;

	UFUNCTION()
	void OnNextPageClicked();

	UFUNCTION()
	void OnPrevPageClicked();

	void UpdateRoomDisplay();
};
