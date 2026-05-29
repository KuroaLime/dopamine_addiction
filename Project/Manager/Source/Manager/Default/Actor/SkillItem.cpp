// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Actor/SkillItem.h"
#include "Components/SphereComponent.h"
#include "Game/InGame/ManagerCharacter.h"

ASkillItem::ASkillItem()
{
    // 종류 설정 (Enum이 있다면)
    ItemType = EPickableType::Skill;
   
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ASkillItem::OnPickedUp(AManagerCharacter* Player)
{
    // [중요 1] 중복 방지 (BaseItem에 있는 변수 활용)
    if (!Player) return;
    if (MeshComp)
    {
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    // 1. 스킬 인벤토리에 추가 (여기가 핵심 차이점)
    SetActorHiddenInGame(true);
    
    Player->AddSkillToInventory(this);

    // 2. 시각적 처리 (숨기기, 물리 끄기)
    // 부모 코드랑 똑같지만, Super를 못 부르니 복사해서 씁니다.
    
    if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionSphere->SetGenerateOverlapEvents(false);
	}
    

    UE_LOG(LogTemp, Log, TEXT("Skill Book Picked Up: %s"), *GetName());
}