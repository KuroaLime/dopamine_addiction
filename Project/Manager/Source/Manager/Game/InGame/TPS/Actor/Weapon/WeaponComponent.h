// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "WeaponComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWeaponComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void Fire(const FVector& MuzzleLocation);

	UFUNCTION(NetMulticast, Reliable)
	virtual void Multicast_PlayFireFeedback(const FVector& MuzzleLocation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Setting")
	EWeaponType WeaponType = EWeaponType::SMG;
public:

	virtual void Reload();
	UFUNCTION(NetMulticast,Reliable)
	virtual void Multicast_PlayReloadFeedback();

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon | Ammo")
	int32 CurrentAmmo=30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Ammo")
	int32 MaxMagazineCapacity = 30;
public:
	UFUNCTION(BlueprintPure, Category = "Weapon | Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon | Ammo")
	int32 GetMaxMagazineCapacity() const { return MaxMagazineCapacity; }

	UFUNCTION(BlueprintCallable, Category = "Weapon | Ammo")
	void ConsumeAmmo();
};
