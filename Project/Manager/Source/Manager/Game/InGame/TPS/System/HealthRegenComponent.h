// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthRegenComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UHealthRegenComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHealthRegenComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
    void OnOwnerTakeDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void StartRegenCooldown();
	void StartRegenLoop();
	void TickRegeneration();
private:
    FTimerHandle RegenCooldownTimerHandle;
    FTimerHandle RegenLoopTimerHandle;


    UPROPERTY(EditDefaultsOnly, Category = "Health Regeneration")
    float CooldownDelay = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Health Regeneration")
    float RegenInterval = 1.0f;

    float FractionalHP = 0.0f;
};
