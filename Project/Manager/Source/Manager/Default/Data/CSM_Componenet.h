// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSM_Componenet.generated.h"

//구현구조
/*
1.클래스 기반 상반신/하반신/전신 상태 객체

2.상태 객체가 입력 처리

3.ASC에 요청

4.성공 시 상태 전이
*/
//enum DownBody//하반신
//{
//	Idle,
//	Walk,
//	Run
//};
//enum UpBody//상반신
//{
//	Idle,
//	Aim,
//	Pick_up,
//	Pick_down,
//};
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UCSM_Componenet : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCSM_Componenet();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
