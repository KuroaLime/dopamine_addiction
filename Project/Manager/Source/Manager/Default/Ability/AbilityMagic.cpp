// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/AbilityMagic.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Default/Ability/CustomASC.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "WorldCollision.h"               // FOverlapResult의 실제 설계도
#include "GameFramework/Actor.h"           // GetActor()를 쓰기 위해 필요
#include "Components/PrimitiveComponent.h"
UAbilityMagic::UAbilityMagic() {
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Magic")));
}


void UAbilityMagic::ActivateAbility() {

    //AManagerCharacter* MC = Cast<AManagerCharacter>(OwnerCharacter);
    //FVector Center = OwnerCharacter->GetActorLocation();

    //TArray<FOverlapResult> Overlaps;
    //FCollisionShape Sphere = FCollisionShape::MakeSphere(100.0f);
    //FCollisionQueryParams QueryParams;
    //QueryParams.AddIgnoredActor(MC);

    //bool bHit = GetWorld()->OverlapMultiByChannel(
    //    Overlaps,
    //    MC->GetActorLocation(),
    //    FQuat::Identity,
    //    ECC_Pawn,
    //    Sphere,
    //    QueryParams
    //);

    //if (bHit)
    //{
    //    for (const FOverlapResult& Result : Overlaps) 
    //    {
    //        AActor* OverlappedActor = Result.GetActor();
    //        if (OverlappedActor)
    //        {
    //            // 여기서 데미지나 이펙트 로직 실행
    //        }
    //    }
    //}

    //EndAbility(false);
}