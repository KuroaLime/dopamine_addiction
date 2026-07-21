// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "A_Spawn.generated.h"

UCLASS()
class MANAGER_API AA_Spawn : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AA_Spawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 SpawnPointID;

	// 상점 페이즈 동안 이동을 막는 방벽. "Barrier" 컴포넌트 태그가 붙은 컴포넌트들을 BeginPlay 시 자동 캐싱.
	// 서버에서만 호출할 것 -> bBarrierActive 리플리케이션을 통해 클라이언트에도 전파됨.
	void SetBarrierActive(bool bActive);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_BarrierActive();

	void ApplyBarrierVisualState();

	// BarrierComponents가 아직 비어있으면(BeginPlay보다 먼저 SetBarrierActive가 불린 경우 등) 즉시 캐싱.
	void EnsureBarrierComponentsCached();

	static const FName BarrierComponentTag;

	bool bBarrierComponentsCached = false;

	UPROPERTY()
	TArray<class UPrimitiveComponent*> BarrierComponents;

	UPROPERTY(ReplicatedUsing = OnRep_BarrierActive)
	bool bBarrierActive = false;

};