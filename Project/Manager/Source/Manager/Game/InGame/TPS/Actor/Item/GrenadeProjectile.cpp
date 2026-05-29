// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Item/GrenadeProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AGrenadeProjectile::AGrenadeProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 1. 충돌 구체 설정 (루트)
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(15.0f);
	CollisionComp->SetCollisionProfileName(TEXT("Projectile")); // Projectile 프리셋 사용 추천
	// 던진 사람 발에 바로 걸리지 않게 설정 (중요)
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	RootComponent = CollisionComp;

	// 2. 메쉬 설정
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌은 구체가 담당

	// 3. 투사체 무브먼트 설정 (포물선, 바운스)
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 1500.f; // 던지는 속도
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->bRotationFollowsVelocity = true; // 날아가는 방향으로 회전
	ProjectileMovement->bShouldBounce = true; // 튀기기 활성화
	ProjectileMovement->Bounciness = 0.3f; // 탄성 (0~1)
	ProjectileMovement->ProjectileGravityScale = 1.0f; // 중력 영향
}

// Called when the game starts or when spawned
void AGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionComp->OnComponentHit.AddDynamic(this, &AGrenadeProjectile::OnHit);
}

void AGrenadeProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode();
}

void AGrenadeProjectile::Explode()
{
	if (bExploded) return;
	bExploded = true;

	// 1. [연출] 이동 멈추고 제자리 고정
	ProjectileMovement->StopMovementImmediately();
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 2. [연출] 수류탄 본체(메쉬) 숨기기 (터졌으니까 사라져야 함)
	MeshComp->SetHiddenInGame(true);

	// 3. [연출] 폭발 이펙트(VFX) & 소리(SFX) 재생
	FVector Location = GetActorLocation();

	// 파티클이 설정되어 있다면 재생
	if (ExplosionVFX)
	{
		// 크기(Scale)나 회전도 조절 가능하지만 지금은 기본값
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, Location, FRotator::ZeroRotator, true);
	}

	// 소리가 설정되어 있다면 재생
	if (ExplosionSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSFX, Location);
	}

	SetLifeSpan(0.5f);
}

// Called every frame
void AGrenadeProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

