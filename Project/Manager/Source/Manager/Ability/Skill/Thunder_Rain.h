// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/CustomAbility.h"
#include "Component/DetectComponent.h"
#include "Thunder_Rain.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UThunder_Rain : public UCustomAbility
{
	GENERATED_BODY()
public:
	UThunder_Rain();

protected:
	virtual void ActivateAbility() override;
public:
	UDetectComponent* Detect;

	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<class UNiagaraSystem> DetectHitFX = nullptr;
private:
	TArray<AActor*> DetectedResult;
	void PlayDetectFXOnTargets(const TArray<AActor*>& Targets);
};
