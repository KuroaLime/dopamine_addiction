// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Protocol_Client/Protocol_D.h"
#include "RoomWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API URoomWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	class UButton* LobbyButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* ReadyButton;

	bool bIsHost = false;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION()
	void OnLobbyButtonClicked();	

	UFUNCTION()
	void OnReadyButtonClicked();

	UFUNCTION()
	void UpdateRoomMemberState(const TArray<FRoomMemberInfoView>& Members);

	void ApplyHostState(bool bNewIsHost);
	void SetReadyButtonLabel(const FText& NewText);
};
