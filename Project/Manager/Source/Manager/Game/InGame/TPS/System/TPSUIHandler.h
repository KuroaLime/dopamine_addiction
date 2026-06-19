// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "TPSUIHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UTPSUIHandler : public UUIHandler
{
	GENERATED_BODY()
	
public:
	UTPSUIHandler();

	virtual void BeginPlay() override;

public:
	virtual void UIActivate() override;
	virtual void UIDeactivate() override;
	virtual void SetUITimer(int32 time) override;

protected:
	virtual void CreateHUD();
	virtual void ShowHUD();
	virtual void HideHUD();
	virtual void UIToggle();
};
