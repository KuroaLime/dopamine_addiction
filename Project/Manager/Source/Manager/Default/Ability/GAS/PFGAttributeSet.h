// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Net/UnrealNetwork.h"
#include "PFGAttributeSet.generated.h"

USTRUCT(BlueprintType)
struct FPFGAttributeData
{
	GENERATED_BODY()

	friend class UPFGAttributeSet;

public:
	float GetCurrentValue() const { return CurrentValue; }
	float GetBaseValue() const { return BaseValue; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AllowPrivateAccess = "true"))
	float BaseValue = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentValue = 0.f;
};

UCLASS(BlueprintType, Blueprintable)
class MANAGER_API UPFGAttributeSet : public UObject
{
	GENERATED_BODY()
	
public:
	UPFGAttributeSet();

	virtual UWorld* GetWorld() const override;

	// SubObject Replication 요구사항:
	// UActorComponent 하위의 UObject(AttributeSet)는 기본적으로 네트워크 복제
	// 대상으로 간주되지 않는다. true를 반환해야 ReplicateSubobjects에서
	// 이 객체를 채널에 등록하고, 이 객체의 Replicated 프로퍼티들이 전송된다.
	virtual bool IsSupportedForNetworking() const override { return true; }


	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Attributes")
	FPFGAttributeData Health;

	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth, BlueprintReadOnly, Category = "Attributes")
	FPFGAttributeData MaxHealth;

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	// 서버 전용: Delta 적용 + Clamp
	void ApplyHealthDelta(float Delta);

	// 서버 전용: MaxHealth/MaxMana 변경 시 CurrentValue를 새 범위로 재조정
	void SetMaxHealth(float NewMaxHealth);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeChanged, float /*OldValue*/, float /*NewValue*/);
	FOnAttributeChanged OnHealthChanged;

protected:
	UFUNCTION()
	void OnRep_Health(FPFGAttributeData OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(FPFGAttributeData OldValue);

private:
	// 내부 전용: Clamp를 강제하는 단일 진입점.
	// Attribute/MaxAttribute 양쪽에서 공통으로 사용해 Clamp 누락을 방지한다.
	static void ClampToMax(FPFGAttributeData& Attribute, const FPFGAttributeData& MaxAttribute);
};
