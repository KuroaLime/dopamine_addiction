// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Component/Player/InteractionComponent.h"
#include "DrawDebugHelpers.h"
#include "Manager.h"
#include "Default/Actor/InteractableInterface.h"
#include "Game/InGame/MainCharacter.h"
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

	// 1. ĳ���Ϳ� ī�޶� ��������
	AMainCharacter* MyChar = Cast<AMainCharacter>(Owner);
	if (!MyChar || !MyChar->GetFollowCamera()) return;

	// 2. ĳ������ ���� �ƴ�, ���� TPS 'ī�޶�'�� ��ġ�� ������ ���������� ����ϴ�.
	FVector Start = MyChar->GetFollowCamera()->GetComponentLocation();
	FRotator Rotation = MyChar->GetFollowCamera()->GetComponentRotation();

	// 3. ī�޶�� ĳ���� �ڿ� �����Ƿ�, ���� TraceDistance(300)�δ� ĳ���� ���� �����ۿ� ���� �ʽ��ϴ�.
	// ���� �������� ȭ�� �߾��� ���� ���� ���(��: 1500) ���ݴϴ�.
	float MaxTraceLength = 1500.0f;
	FVector End = Start + (Rotation.Vector() * MaxTraceLength);

	DS_DRAW_LINE(GetWorld(), Start, End, FColor::Green, false, -1.f, 0, 1.0f);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner); // �� ĳ���ʹ� ����

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		DS_DRAW_POINT(GetWorld(), HitResult.ImpactPoint, 10.f, FColor::Red, false, -1.f);

		// 4. [�ٽ�] ī�޶� �������� ���� ������ 'ĳ������ ��ġ'�κ��� ��ȣ�ۿ� ������ �Ÿ�(TraceDistance) ���� �ִ��� �˻��մϴ�.
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
				return; // ���������� �������� ã�����Ƿ� ���⼭ ����
			}
		}
	}

	// 5. ����� ���ų�, �������� ����(TraceDistance) ���̸� ��Ŀ���� �����մϴ�.
	if (FocusedActor)
	{
		IInteractableInterface::Execute_OnEndFocus(FocusedActor);
		FocusedActor = nullptr;
	}

}