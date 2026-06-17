// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

UENUM(BlueprintType)
enum class EFireType : uint8 {
	LineTrace,
	Projectile
};

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
	virtual void FireOnce();
	virtual void StartLoopFire();
	virtual void StopLoopFire();
	virtual void Reload();

protected:
	FTimerHandle FireTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float FireInterval = 0.1f;
	//Fire시 나오는 Sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_FireSound;

	//탄알이 비었을 시 나오는 Sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_pEmptySound;

	//모든 소리를 저장하는 용도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TArray<USoundBase*> m_pEnvironmentalSounds;


	//레이저 종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazer")
	EFireType FireType = EFireType::LineTrace;

	//socket 대용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	FVector m_vMuzzleOffset = FVector(100.0f, 0, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float Reload_Time = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float MaxRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float Damage = 10.0f;
};
