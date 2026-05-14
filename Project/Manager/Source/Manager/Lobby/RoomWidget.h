// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

	virtual void NativeConstruct() override;
private:
	UFUNCTION()
	void OnLobbyButtonClicked();	
};
