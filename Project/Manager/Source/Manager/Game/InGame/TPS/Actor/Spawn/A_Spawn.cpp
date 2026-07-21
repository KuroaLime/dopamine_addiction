// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"

const FName AA_Spawn::BarrierComponentTag(TEXT("Barrier"));

// Sets default values
AA_Spawn::AA_Spawn()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BODY"));
	RootComponent = Body;

	bReplicates = true;
	bAlwaysRelevant = true; // 스폰 지점 개수가 적으므로 릴레번시 컬링 없이 항상 모든 클라이언트에 복제
}

// Called when the game starts or when spawned
void AA_Spawn::BeginPlay()
{
	Super::BeginPlay();

	EnsureBarrierComponentsCached();
	ApplyBarrierVisualState(); // 상점 페이즈 진입 전까지는 항상 비활성 상태(bBarrierActive 기본값 false)로 시작
}

// Called every frame
void AA_Spawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AA_Spawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AA_Spawn, bBarrierActive);
}

void AA_Spawn::SetBarrierActive(bool bActive)
{
	// 서버(권위)에서 호출. bBarrierActive 변경은 클라이언트로 리플리케이트되어 OnRep_BarrierActive를 통해 적용됨.
	bBarrierActive = bActive;
	EnsureBarrierComponentsCached(); // BeginPlay보다 먼저 호출된 경우를 대비한 방어 캐싱
	ApplyBarrierVisualState(); // 서버 자신은 OnRep이 호출되지 않으므로 로컬에도 즉시 적용
}

void AA_Spawn::OnRep_BarrierActive()
{
	EnsureBarrierComponentsCached();
	ApplyBarrierVisualState();
}

void AA_Spawn::EnsureBarrierComponentsCached()
{
	if (bBarrierComponentsCached)
	{
		return;
	}
	bBarrierComponentsCached = true;

	TArray<UPrimitiveComponent*> AllPrimitives;
	GetComponents<UPrimitiveComponent>(AllPrimitives);
	for (UPrimitiveComponent* Comp : AllPrimitives)
	{
		if (Comp && Comp->ComponentHasTag(BarrierComponentTag))
		{
			BarrierComponents.Add(Comp);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[A_Spawn] %s: Cached %d barrier component(s)"),
		*GetName(), BarrierComponents.Num());
}

void AA_Spawn::ApplyBarrierVisualState()
{
	UE_LOG(LogTemp, Warning, TEXT("[A_Spawn] %s: ApplyBarrierVisualState bActive=%d ComponentCount=%d HasAuthority=%d NetMode=%d"),
		*GetName(), bBarrierActive ? 1 : 0, BarrierComponents.Num(), HasAuthority() ? 1 : 0, (int32)GetWorld()->GetNetMode());

	for (UPrimitiveComponent* Comp : BarrierComponents)
	{
		if (Comp)
		{
			// 콜리전이 켜질 때(방벽이 실제로 막고 있을 때) 메시도 같이 보이도록 한다.
			Comp->SetVisibility(bBarrierActive, false);
			Comp->SetCollisionEnabled(bBarrierActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			UE_LOG(LogTemp, Warning, TEXT("[A_Spawn]   -> %s Visible=%d CollisionEnabled=%d"),
				*Comp->GetName(), Comp->IsVisible() ? 1 : 0, (int32)Comp->GetCollisionEnabled());
		}
	}
}

