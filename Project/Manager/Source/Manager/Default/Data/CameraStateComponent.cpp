// Fill out your copyright notice in the Description page of Project Settings.
#include "Default/Data/CameraStateComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"


// Sets default values for this component's properties
UCameraStateComponent::UCameraStateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCameraStateComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = Cast<ATPSCharacter>(GetOwner());
	if (!OwnerActor) return;

	OwnerArm = OwnerActor->GetCameraBoom();
	OwnerCamera = OwnerActor->GetFollowCamera();
	
}


// Called every frame
void UCameraStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
void UCameraStateComponent::SmoothZoom(bool bZoomIn)
{

	const float TargetFOV = bZoomIn ? 60.0f : 90.0f;
	const float TargetArmLength = bZoomIn ? 200.0f : 400.0f;
	const FVector TargetOffset = bZoomIn ? FVector(0.0f, 80.0f, 60.0f) : FVector(0.0f, 75.0f, 50.0f);

	GetWorld()->GetTimerManager().ClearTimer(ZoomTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(ZoomTimerHandle, FTimerDelegate::CreateLambda([this, TargetFOV, TargetArmLength, TargetOffset]() {
		if (!OwnerCamera || !OwnerArm) return;

		float CurrentFOV = OwnerCamera->FieldOfView;
		float CurrentArmLength = OwnerArm->TargetArmLength;
		FVector CurrentOffset = OwnerArm->SocketOffset;

		float NextFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, 0.01f, 10.0f);
		float NextArmLength = FMath::FInterpTo(CurrentArmLength, TargetArmLength, 0.01f, 10.0f);
		FVector NextOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, 0.01f, 10.0f);

		OwnerCamera->SetFieldOfView(NextFOV);
		OwnerArm->TargetArmLength = NextArmLength;
		OwnerArm->SocketOffset = NextOffset;

		if (FMath::IsNearlyEqual(NextFOV, TargetFOV, 0.1f))
		{
			OwnerCamera->SetFieldOfView(TargetFOV);
			OwnerArm->TargetArmLength = TargetArmLength;
			OwnerArm->SocketOffset = TargetOffset;
			GetWorld()->GetTimerManager().ClearTimer(ZoomTimerHandle);
		}
		}), 0.01f, true);
}
