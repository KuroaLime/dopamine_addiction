// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Ability_CardDiscard.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UAbility_CardDiscard : public UPFGAbility
{
	GENERATED_BODY()
public:
	UAbility_CardDiscard();

public:
	virtual void LocalActivateWithOwner(AActor* InOwner) override;
	virtual void LocalCancelWithOwner(AActor* InOwner) override;

protected:
	virtual void ActivateAbility() override;
	virtual void EndAbility(bool bWasCancelled) override;

private:
	void bDiscard(AActor* InOwner, bool isAim);
	void NextCardSelection(AActor* InOwner, bool isAim);

private:
	FTimerHandle HoldConfirmTimerHandle;
	bool bHoldConfirmed = false;

	UPROPERTY(EditDefaultsOnly, Category = "Discard")
	float HoldThreshold = 0.6f;

	void StartHold(AActor* InOwner, UWorld* World);
	void CancelHold(AActor* InOwner, UWorld* World);
	void ConfirmHold();

	TWeakObjectPtr<AActor> HoldOwner;
	class UTpsPlayerMainHUD* ResolveHUD(AActor* InOwner) const;
};
