// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "CardUIHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UCardUIHandler : public UUIHandler
{
	GENERATED_BODY()
	
public:
	UCardUIHandler();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	virtual void UIActivate() override;
	virtual void UIDeactivate() override;
};
