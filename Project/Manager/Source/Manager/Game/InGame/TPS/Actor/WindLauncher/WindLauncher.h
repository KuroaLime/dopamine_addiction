#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"   // EMovementMode
#include "WindLauncher.generated.h"

class UBoxComponent;
class ACharacter;

UCLASS()
class MANAGER_API AWindLauncher : public AActor
{
	GENERATED_BODY()

public:
	AWindLauncher();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Launcher")
	USceneComponent* RootComp;

	// 플레이어가 들어오면 "점프 시" 발사될 범위를 지정하는 트리거 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Launcher")
	UBoxComponent* WindTrigger;

	// 발사되어 도착할 목표 섬의 지점 액터 (에디터에서 스포이드로 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher")
	AActor* TargetIslandPoint;

	// 발사 중 공중 조작력 (0=조작 불가, 1=완전 조작). 비행하는 동안 착지 지점을 플레이어가 고를 수 있게 한다.
	// 착지하면 캐릭터 원래 값으로 자동 복원.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LaunchAirControl = 1.0f;

	//==== 아크(포물선) 자동 계산: 플레이어↔목표 거리/높이로 힘을 정함. 항상 목표에 착지 ====
	// 수평 거리에 비례해 아크 정점을 얼마나 높일지. 멀수록 더 높고 세게 솟는다. (정점높이 = 거리 × 이 값)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher|Arc", meta = (ClampMin = "0.0"))
	float ApexHeightRatio = 0.6f;

	// 아크 정점 최소 높이(cm). 가까운 목표라도 최소 이만큼은 위로 솟게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher|Arc", meta = (ClampMin = "0.0"))
	float MinApexHeight = 500.f;

	// 아크 정점 최대 높이(cm). 아주 먼 목표일 때 과도하게 솟지 않도록 상한.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher|Arc", meta = (ClampMin = "0.0"))
	float MaxApexHeight = 3000.f;

	// 수직 발사력 배율. 1보다 크면 더 높고 세게 솟는다(체공↑). 수평속도는 자동 보정돼 목표엔 그대로 착지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Launcher|Arc", meta = (ClampMin = "1.0"))
	float VerticalPowerScale = 1.4f;

protected:
	virtual void BeginPlay() override;

	// 범위 진입/이탈 — 진입한 플레이어의 이동모드 변경을 구독/해제한다
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 범위 안 플레이어가 이동모드 변경(=점프로 Falling 진입) 시 호출 → 점프면 발사
	UFUNCTION()
	void OnCharacterMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	// 실제 발사 (서버 권위 + 조종 클라 예측)
	void LaunchPlayer(ACharacter* PlayerCharacter);

	// 발사 성공 시 호출 — BP에서 사운드/VFX 등 연출을 붙이는 훅
	UFUNCTION(BlueprintImplementableEvent, Category = "Wind Launcher")
	void OnPlayerLaunched(ACharacter* LaunchedCharacter);

private:
	// 비행 중 캐릭터의 발사 전 무브먼트 값 (착지 시 복원용)
	struct FWindLaunchSaved
	{
		float AirControl = 0.35f;
		float BrakingDecelerationFalling = 0.f;
	};

	// 비행 중인 캐릭터 → 저장된 값. 런타임 상태라 비직렬화.
	TMap<TWeakObjectPtr<ACharacter>, FWindLaunchSaved> InFlight;
};
