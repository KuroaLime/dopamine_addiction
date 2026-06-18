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
	virtual void Fire(const FVector& MuzzleLocation, const FVector& ShotDirection, float InDamage = -1.0f);
	virtual void FireOnce();
	void PlayLocalFireFeedback();
	void ExecuteServerFireFromView(FVector ViewLocation, FRotator ViewRotation);
	virtual void StartLoopFire();
	virtual void StopLoopFire();
	virtual void Reload();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_FireFromClient(FVector ViewLocation, FRotator ViewRotation);

protected:
	FTimerHandle FireTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float FireInterval = 0.1f;
	//Fire�� ������ Sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_FireSound;

	//ź���� ����� �� ������ Sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* m_pEmptySound;

	//��� �Ҹ��� �����ϴ� �뵵
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TArray<USoundBase*> m_pEnvironmentalSounds;


	//������ ����
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazer")
	EFireType FireType = EFireType::LineTrace;

	//socket ���
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	FVector m_vMuzzleOffset = FVector(100.0f, 0, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float Reload_Time = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float MaxRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float Damage = 10.0f;
};
