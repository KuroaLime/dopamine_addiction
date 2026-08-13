// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GoldenGoblinCharacter.generated.h"

class UPFGASC;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGoblinHealthChangedBP, float, CurrentHealth, float, MaxHealth, float, HealthRatio);

/**
 * 배회하다 처치하면 골드를 드랍하는 회피/도주형 레어 몬스터.
 * 체력은 플레이어와 동일한 UPFGASC/UPFGAttributeSet을 재사용한다.
 */
UCLASS()
class MANAGER_API AGoldenGoblinCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGoldenGoblinCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	bool IsGoblinDead() const { return bIsDead; }
	int32 GetGoldRewardAmount() const { return RolledGoldRewardAmount; }

	float GetPatrolMoveSpeed() const { return PatrolMoveSpeed; }
	float GetEvasiveMoveSpeed() const { return EvasiveMoveSpeed; }
	void SetGoblinMoveSpeed(float NewSpeed);

	// AIController가 바인딩해서 Blackboard의 IsDead 키를 갱신하는 데 사용.
	DECLARE_MULTICAST_DELEGATE(FOnGoblinDied);
	FOnGoblinDied OnGoblinDied;

	// HP 위젯이 이 이벤트를 바인딩하면 체력 변화(서버/클라 양쪽)마다 자동으로 알림을 받는다.
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnGoblinHealthChangedBP OnGoblinHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthRatio() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMaxHealthValue() const;

	// 데디케이티드 서버는 렌더링을 안 하고 SpawnSystemAtLocation은 리플리케이트되지 않으므로,
	// BTT_Disappear(서버에서만 실행)가 이 함수를 호출해 모든 클라이언트에서 각자 로컬로 스폰하게 한다.
	UFUNCTION(NetMulticast, Reliable, Category = "VFX")
	void Multicast_PlayDeathEffect(UNiagaraSystem* Effect, FVector Location);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UPFGASC* AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = "Stats")
	float GoblinMaxHealth = 60.f;

	// 평소 배회 이동속도.
	UPROPERTY(EditAnywhere, Category = "Stats")
	float PatrolMoveSpeed = 350.f;

	// 도주 중 부스트 이동속도. BTT_EvasiveManeuver가 참조.
	UPROPERTY(EditAnywhere, Category = "Stats")
	float EvasiveMoveSpeed = 700.f;

	// 이 범위 내에서 무작위 골드 보상을 굴린다. 고정값이면 "이 몬스터만 잡으면 확정으로 이긴다"는
	// 식으로 공략이 고정돼버리는 걸 막기 위함.
	UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0"))
	int32 MinGoldRewardAmount = 100;

	UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0"))
	int32 MaxGoldRewardAmount = 3000;

private:
	void HandleDeath();
	void TryBindHealthDelegate();
	void HandleHealthChanged(float OldValue, float NewValue);
	void UpdateHPWidgetVisibility();

	bool bIsDead = false;
	FTimerHandle HealthBindRetryTimer;

	// BeginPlay(서버)에서 Min~MaxGoldRewardAmount 사이로 한 번 굴려서 고정해둔 값.
	int32 RolledGoldRewardAmount = 0;
};
