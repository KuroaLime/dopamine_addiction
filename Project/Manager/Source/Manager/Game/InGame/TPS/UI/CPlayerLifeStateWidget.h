// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "CPlayerLifeStateWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UCPlayerLifeStateWidget : public UWidgetParent
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

protected:
	void UpdatePSU_BackgroundImage();
	void UpdatePlayerImage();

private:

	//배경
	UPROPERTY()
	class UImage* PSU_Background[2];
	//플레이어 아이콘
	UPROPERTY()
	class UImage* Player[4];
};
