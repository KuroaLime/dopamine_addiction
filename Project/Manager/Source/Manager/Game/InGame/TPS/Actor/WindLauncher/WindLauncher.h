#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindLauncher.generated.h"

class UBoxComponent;

UCLASS()
class MANAGER_API AWindLauncher : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindLauncher();

protected:
	virtual void BeginPlay() override;

public:	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Launcher")
	USceneComponent* RootComp;

	// 플레이어가 닿으면 발사될 범위를 지정하는 트리거 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Launcher")
	UBoxComponent* WindTrigger;

	// 발사되어 도착할 목표 섬의 지점 액터 (에디터에서 스포이드로 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher")
	AActor* TargetIslandPoint;

	// 충돌 감지 이벤트
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
