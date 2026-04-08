// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/DetectComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Engine/OverlapResult.h"

// Sets default values for this component's properties
UDetectComponent::UDetectComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UDetectComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

TArray<AActor*> UDetectComponent::Detect(const FVector& Center, float Radius) {
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return {};

    UWorld* World = GetWorld();
    if (!World) return {};

    if (bDebugDraw)
    {
        DrawDebugSphere(World, Center, Radius, 16, FColor::Green, false, 1.0f);
    }

    TArray<FOverlapResult> Overlaps;

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn); // 적이 Pawn/Character라면 보통 이거

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DetectEnemies), false);
    QueryParams.AddIgnoredActor(const_cast<AActor*>(OwnerActor));

    const bool bHit = World->OverlapMultiByObjectType(
        Overlaps,
        Center,
        FQuat::Identity,
        ObjParams,
        FCollisionShape::MakeSphere(Radius),
        QueryParams
    );

    if (!bHit) return {};

    TArray<AActor*> Result;
    Result.Reserve(Overlaps.Num());

    for (const FOverlapResult& R : Overlaps)
    {
        AActor* Target = R.GetActor();
        if (!Target) continue;

        if (!IsEnemy(OwnerActor, Target)) continue;
        if (bCheckLOS && !HasLineOfSight(OwnerActor, Target)) continue;

        Result.Add(Target);

        if (MaxTargets > 0 && Result.Num() >= MaxTargets)
            break;
    }

    return Result;
}
//탐지한 객체가 적인지 아군인지 판정
bool UDetectComponent::IsEnemy(const AActor* Source, const AActor* Target) const {
    return Source != Target;
}
//디버그용 선 그리기
bool UDetectComponent::HasLineOfSight(const AActor* Source, const AActor* Target) const {

    UWorld* World = GetWorld();
    if (!World) return false;

    const FVector From = Source->GetActorLocation() + FVector(0, 0, Offset);
    const FVector To = Target->GetActorLocation() + FVector(0, 0, Offset);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DetectLOS), true);
    Params.AddIgnoredActor(const_cast<AActor*>(Source));
    Params.AddIgnoredActor(const_cast<AActor*>(Target));

    const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, To, LOSTraceChannel, Params);

    if (bDebugDraw)
    {
        DrawDebugLine(World, From, To, bBlocked ? FColor::Red : FColor::Cyan, false, 1.0f, 0, 1.5f);
    }

    return !bBlocked;
}
