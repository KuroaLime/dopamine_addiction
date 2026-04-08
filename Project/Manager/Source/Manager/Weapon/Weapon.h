// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "NiagaraFunctionLibrary.h"
#include "Weapon/WeaponComponent.h"
#include "Weapon.generated.h"




UCLASS()
class MANAGER_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeapon();

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	//UNiagaraSystem* m_pMuzzleFlash;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleDefaultsOnly, Category = "Mesh")
	UStaticMeshComponent* m_pMesh;

	UPROPERTY(VisibleDefaultsOnly, Category = "Weapon Setting")
	UWeaponComponent* Setting;
};

