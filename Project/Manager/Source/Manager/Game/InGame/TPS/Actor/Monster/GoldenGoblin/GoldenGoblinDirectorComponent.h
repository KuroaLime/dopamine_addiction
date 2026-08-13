// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GoldenGoblinDirectorComponent.generated.h"

class AGoldenGoblinCharacter;

/**
 * 황금 고블린의 "언제/어디에 등장시킬 것인가"만 담당하는 페이싱 레이어.
 * 개별 고블린의 행동(Behavior Tree)과는 완전히 분리되어 있다.
 * GameMode가 소유하며 BattleRoyale 페이즈 동안만 활성화된다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MANAGER_API UGoldenGoblinDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGoldenGoblinDirectorComponent();

	// 현재 (서버 권위) 게임모드에 붙은 디렉터를 찾아 반환. 없으면 nullptr.
	static UGoldenGoblinDirectorComponent* GetActive(const UObject* WorldContextObject);

	// BattleRoyale 페이즈 시작/종료 시 GameMode가 호출.
	void ActivateForBattleRoyale();
	void Deactivate();

	// 전투 이벤트(데미지 등) 발생 시 호출 → "소강 상태" 판단의 기준 시각을 갱신.
	void NotifyCombatEvent();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin")
	TSubclassOf<AGoldenGoblinCharacter> GoblinClass;

	// 페이즈 시작 후 이 시간이 지나야 스폰을 고려한다.
	// TODO: 테스트 편의상 10초로 낮춰둠 — 실제 밸런싱 시 60초 이상으로 되돌릴 것.
	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "0.0"))
	float MinSpawnDelaySeconds = 10.f;

	// 최근 전투 이벤트가 이 시간 이상 없어야 "소강 상태"로 판단한다.
	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "0.0"))
	float CombatLullSeconds = 8.f;

	// 스폰 지점이 모든 플레이어로부터 최소 이만큼은 떨어져 있어야 한다.
	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "0.0"))
	float MinDistanceFromPlayers = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "0.0"))
	float SpawnSearchRadius = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "1"))
	int32 MaxSpawnAttempts = 8;

	// 조건 재검사 주기.
	UPROPERTY(EditDefaultsOnly, Category = "GoldenGoblin", meta = (ClampMin = "0.5"))
	float CheckIntervalSeconds = 2.f;

private:
	void TrySpawnGoblin();

	bool bActive = false;
	bool bSpawnedThisPhase = false;
	double PhaseStartTimeSeconds = 0.0;
	double LastCombatEventTimeSeconds = 0.0;

	FTimerHandle CheckTimerHandle;
};
