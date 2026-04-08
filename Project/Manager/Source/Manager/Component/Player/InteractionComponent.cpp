// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/Player/InteractionComponent.h"
#include "DrawDebugHelpers.h"
#include "Items/IDLE/InteractableInterface.h"
#include "ManagerCharacter.h"
#include "Camera/CameraComponent.h"
// Sets default values for this component's properties
UInteractionComponent::UInteractionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	
}


// Called when the game starts
void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (NearbyInteractableCount > 0) PerformLineTrace();
	else if (FocusedActor) {
		IInteractableInterface::Execute_OnEndFocus(FocusedActor);
		FocusedActor = nullptr;
	}
}

void UInteractionComponent::AddNearByInteractable(int32 Amount) {
	NearbyInteractableCount += Amount;
}

AActor* UInteractionComponent::GetFocusedActor() const {
	return FocusedActor;
}
void UInteractionComponent::PerformLineTrace() {

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// 1. 캐릭터와 카메라 가져오기
	AManagerCharacter* MyChar = Cast<AManagerCharacter>(Owner);
	if (!MyChar || !MyChar->GetFollowCamera()) return;

	// 2. 캐릭터의 눈이 아닌, 실제 TPS '카메라'의 위치와 방향을 시작점으로 잡습니다.
	FVector Start = MyChar->GetFollowCamera()->GetComponentLocation();
	FRotator Rotation = MyChar->GetFollowCamera()->GetComponentRotation();

	// 3. 카메라는 캐릭터 뒤에 있으므로, 기존 TraceDistance(300)로는 캐릭터 앞의 아이템에 닿지 않습니다.
	// 따라서 레이저를 화면 중앙을 향해 아주 길게(예: 1500) 쏴줍니다.
	float MaxTraceLength = 1500.0f;
	FVector End = Start + (Rotation.Vector() * MaxTraceLength);

	DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, -1.f, 0, 1.0f);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner); // 내 캐릭터는 무시

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.f, FColor::Red, false, -1.f);

		// 4. [핵심] 카메라 레이저가 맞은 지점이 '캐릭터의 위치'로부터 상호작용 가능한 거리(TraceDistance) 내에 있는지 검사합니다.
		float DistanceToPlayer = FVector::Dist(Owner->GetActorLocation(), HitResult.ImpactPoint);

		if (DistanceToPlayer <= TraceDistance)
		{
			AActor* HitActor = HitResult.GetActor();
			if (HitActor && HitActor->Implements<UInteractableInterface>())
			{
				if (HitActor != FocusedActor)
				{
					if (FocusedActor) IInteractableInterface::Execute_OnEndFocus(FocusedActor);
					FocusedActor = HitActor;
					IInteractableInterface::Execute_OnBeginFocus(FocusedActor);
				}
				return; // 성공적으로 아이템을 찾았으므로 여기서 종료
			}
		}
	}

	// 5. 허공을 보거나, 아이템이 범위(TraceDistance) 밖이면 포커스를 해제합니다.
	if (FocusedActor)
	{
		IInteractableInterface::Execute_OnEndFocus(FocusedActor);
		FocusedActor = nullptr;
	}

}