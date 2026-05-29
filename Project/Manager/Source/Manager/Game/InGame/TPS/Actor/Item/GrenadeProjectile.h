// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrenadeProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class MANAGER_API AGrenadeProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGrenadeProjectile();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Explode();

protected:
	// --- 컴포넌트 ---
	// 충돌 감지용 구체 (루트 컴포넌트가 됩니다)
	UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
	USphereComponent* CollisionComp;

	// 눈에 보이는 메쉬
	UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
	UStaticMeshComponent* MeshComp;

	// [핵심] 투사체 움직임을 담당하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	UProjectileMovementComponent* ProjectileMovement;

	// --- 설정값 ---
	// 폭발 후 사라지기까지 걸리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Grenade Stats")
	float DestroyDelay = 1.0f;

	// 폭발 반경
	UPROPERTY(EditDefaultsOnly, Category = "Grenade Stats")
	float ExplosionRadius = 300.0f;

	// 폭발 파티클 및 사운드 (선택사항)
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ExplosionVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	USoundBase* ExplosionSFX;

	bool bExploded = false;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
