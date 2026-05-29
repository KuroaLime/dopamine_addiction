// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoomUserWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API URoomUserWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* UserNameText;
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* UserStateText;

public:
	void UpdateUserInfo(const FString& UserName, bool bIsReady);
};
