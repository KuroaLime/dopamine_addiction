// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "CharacterStateComponent.generated.h"

#define MAX_GOLD 10000
//PLAYER IMAGE
DECLARE_MULTICAST_DELEGATE(FOnPlayerImageChangedDelegate);
//HP
DECLARE_MULTICAST_DELEGATE(FOnHPChangedDelegate);
DECLARE_MULTICAST_DELEGATE(FOnHPISZeroDelegate);

//LEVEL
DECLARE_MULTICAST_DELEGATE(FOnLEVELChangedDelegate);
//NAME
DECLARE_MULTICAST_DELEGATE(FOnNameChangedDelegate);

//SKILL
DECLARE_MULTICAST_DELEGATE(FOnSkillStateChangedDelegate);

//WEAPON IMAGE
DECLARE_MULTICAST_DELEGATE(FOnWeaponStateChangedDelegate);
//WEAPON COUNT
DECLARE_MULTICAST_DELEGATE(FOnWeaponCountChangedDelegate);


//Compass
DECLARE_MULTICAST_DELEGATE(FOnComapassChangedDelegate);
//Gold
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoldChangeDelegate, float New);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UCharacterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCharacterStateComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void SetNewLevel(int32 NewLevel);

	float GetHPRatio();
	float GetMaxHP();
	float GetCurrentHP();
	float GetAttack();

	int GetLevel();

	//PLAYER IMAGE
	FOnPlayerImageChangedDelegate OnPlayerImageChanged;
	//HP
	FOnHPISZeroDelegate OnHPIsZero;
	FOnHPChangedDelegate OnHPChanged;
	//LEVEL
	FOnLEVELChangedDelegate OnLEVELChanged;
	//NAME
	FOnNameChangedDelegate OnNameChanged;
	//SKILL
	FOnSkillStateChangedDelegate OnSkillStateChanged;
	//WEAPON IMAGE
	FOnWeaponStateChangedDelegate OnWeaponStateChanged;
	//WEAPON COUNT
	FOnWeaponCountChangedDelegate OnWeaponCountChanged;

	//Compass
	FOnComapassChangedDelegate OnCompassChanged;
	//Gold
	FOnGoldChangeDelegate OnGoldChanged;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	struct FABCharacterData* CurrentStateData = nullptr;

protected:
	//Level
	UPROPERTY(ReplicatedUsing = OnRep_Level, EditInstanceOnly, Category = State, Meta = (AllowPrivateAccess = true))
	int32 Level;

	UFUNCTION()
	void OnRep_Level();

	//Gold
	//UPROPERTY(ReplicatedUsing = OnRep_HoldingGold, Transient, VisibleInstanceOnly, Category = State, Meta = (AllowPrivateAccess = true))
	//float HoldingGold;
	UFUNCTION()
	void OnRep_HoldingGold(float NewGold);

	UFUNCTION()
	void OnRep_ChangeCurrentHP(float NewHP);

public:
	UFUNCTION()
	void BindToPlayerState(class AMainPlayerState* PS);
};
