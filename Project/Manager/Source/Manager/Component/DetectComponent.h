// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DetectComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UDetectComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDetectComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	UFUNCTION(BlueprintCallable,Category ="Detect")
	TArray<AActor*> Detect(const FVector& Center, float Radius);

private:
	UPROPERTY(EditAnywhere, Category = "DetectComponent")
	bool bCheckLOS = false;

	UPROPERTY(EditAnywhere, Category = "DetectComponent")
	TEnumAsByte<ECollisionChannel> LOSTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "DetectComponent")
	int32 MaxTargets = 0;
	UPROPERTY(EditAnywhere, Category = "DetectComponent")
	float Offset = 60.0f;

	UPROPERTY(EditAnywhere, Category = "DetectComponent")
	bool bDebugDraw = false;
private:

	//탐지한 객체가 적인지 아군인지 판정
	bool IsEnemy(const AActor* Source, const AActor* Target) const;
	//디버그용 선 그리기
	bool HasLineOfSight(const AActor* Source, const AActor* Target) const;

};
