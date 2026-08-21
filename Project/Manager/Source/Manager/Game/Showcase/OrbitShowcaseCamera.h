// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitShowcaseCamera.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/**
 * 게임 소개 영상 촬영용 궤도(orbit) 카메라.
 *
 * 액터의 위치가 곧 궤도의 중심(피벗)이며, 스프링암이 피벗을 계속 바라보므로
 * 카메라는 항상 중심을 향한 채 일정한 속도로 주위를 돈다.
 *
 * bAutoFrameOnBeginPlay가 켜져 있으면 BeginPlay에서 레벨의 렌더링 가능한
 * 액터 전체 바운드를 계산해 피벗과 반경을 자동으로 맞춘다. 따라서 섬 좌표를
 * 몰라도 배치만 하면 전체가 화면에 담긴다.
 *
 * 게임플레이 로직/네트워크와 완전히 무관하며 오직 뷰 타깃으로만 쓰인다.
 */
UCLASS()
class MANAGER_API AOrbitShowcaseCamera : public AActor
{
	GENERATED_BODY()

public:
	AOrbitShowcaseCamera();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 레벨 바운드를 지금 계산해서 피벗 위치와 궤도 반경을 맞춘다.
	 * 디테일 패널의 버튼으로 눌러 에디터에서 즉시 구도를 확인할 수 있다.
	 */
	UFUNCTION(CallInEditor, Category = "Orbit Showcase|Framing")
	void FrameLevelNow();

	/** 현재 설정값을 스프링암/카메라에 반영한다. */
	void ApplyCameraTransform();

protected:
	virtual void BeginPlay() override;

	/**
	 * 레벨에서 화면에 보이는 프리미티브들의 합집합 바운드를 구한다.
	 * 스카이 스피어처럼 비정상적으로 큰 프리미티브는 반경 기준으로 제외한다.
	 */
	bool ComputeLevelBounds(FBox& OutBounds) const;

public:
	// ===== 구도 자동 맞춤 =====

	/** BeginPlay에서 레벨 바운드로 피벗·반경을 자동 계산할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Framing")
	bool bAutoFrameOnBeginPlay = true;

	/** 자동 맞춤 시 여유 배율. 1보다 크면 피사체가 화면에서 더 작아진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Framing", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float FramingPadding = 1.15f;

	/** 자동 맞춤으로 잡은 중심에서 위로 더 올릴 높이. 지형 중심이 낮게 잡힐 때 보정용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Framing")
	float FramingPivotZOffset = 0.0f;

	/**
	 * 이 태그가 비어 있지 않으면 해당 액터 태그를 가진 액터만으로 바운드를 계산한다.
	 * 섬 하나만 잡고 싶을 때 대상 액터에 태그를 붙여 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Framing")
	FName FramingActorTag = NAME_None;

	/**
	 * 이 반경(cm)을 넘는 프리미티브는 바운드 계산에서 제외한다.
	 * 스카이 스피어·대기 액터가 구도를 망치는 것을 막는다. 기본 5km.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Framing", meta = (ClampMin = "1000.0"))
	float MaxPrimitiveRadiusForFraming = 500000.0f;

	// ===== 궤도 운동 =====

	/** 피벗에서 카메라까지의 거리(cm). 자동 맞춤이 켜져 있으면 BeginPlay에서 덮어쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion", meta = (ClampMin = "100.0"))
	float OrbitRadius = 30000.0f;

	/** 내려다보는 각도(도). 0이면 수평, 90이면 완전한 부감. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float OrbitPitchDegrees = 22.0f;

	/** 피벗을 기준으로 궤도면을 위아래로 옮기는 오프셋(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion")
	float OrbitHeightOffset = 0.0f;

	/** 시작 방위각(도). 녹화 시작 구도를 정할 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float StartAzimuthDegrees = 0.0f;

	/**
	 * 초당 회전 각도(도). 3이면 한 바퀴에 120초.
	 * 소개 영상은 느릴수록 안정적으로 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion")
	float OrbitSpeedDegreesPerSecond = 3.0f;

	/** 회전 방향을 반대로 뒤집는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion")
	bool bReverseOrbit = false;

	// ===== 부가 움직임 =====

	/** 높이를 사인파로 흔들어 단조로움을 줄인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Bob")
	bool bEnableHeightBob = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Bob", meta = (EditCondition = "bEnableHeightBob"))
	float HeightBobAmplitude = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Bob", meta = (EditCondition = "bEnableHeightBob", ClampMin = "0.1"))
	float HeightBobPeriodSeconds = 24.0f;

	/** 반경을 사인파로 늘였다 줄이며 천천히 다가갔다 물러난다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Dolly")
	bool bEnableRadiusPulse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Dolly", meta = (EditCondition = "bEnableRadiusPulse"))
	float RadiusPulseAmplitude = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Motion|Dolly", meta = (EditCondition = "bEnableRadiusPulse", ClampMin = "0.1"))
	float RadiusPulsePeriodSeconds = 36.0f;

	// ===== 카메라 =====

	/** 수평 화각(도). 자동 맞춤 계산에도 함께 쓰인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbit Showcase|Camera", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FieldOfView = 70.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Orbit Showcase|Components")
	TObjectPtr<USceneComponent> PivotRoot;

	UPROPERTY(VisibleAnywhere, Category = "Orbit Showcase|Components")
	TObjectPtr<USpringArmComponent> OrbitArm;

	UPROPERTY(VisibleAnywhere, Category = "Orbit Showcase|Components")
	TObjectPtr<UCameraComponent> ShowcaseCamera;

private:
	/** 현재 방위각(도). BeginPlay에서 StartAzimuthDegrees로 초기화된다. */
	float CurrentAzimuthDegrees = 0.0f;

	/** 흔들림/펄스 위상 계산용 누적 시간(초). */
	float ElapsedSeconds = 0.0f;
};
