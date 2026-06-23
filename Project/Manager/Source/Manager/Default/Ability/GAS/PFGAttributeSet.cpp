// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/GAS/PFGAttributeSet.h"
#include "Default/Ability/GAS/PFGASC.h"

UPFGAttributeSet::UPFGAttributeSet()
{
	MaxHealth.BaseValue = MaxHealth.CurrentValue = 100.f;
	Health.BaseValue = Health.CurrentValue = 100.f;
}

UWorld* UPFGAttributeSet::GetWorld() const
{
	if (UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}

void UPFGAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UPFGAttributeSet, Health);
	DOREPLIFETIME(UPFGAttributeSet, MaxHealth);
}

void UPFGAttributeSet::ClampToMax(FPFGAttributeData& Attribute, const FPFGAttributeData& MaxAttribute)
{
	Attribute.CurrentValue = FMath::Clamp(Attribute.CurrentValue, 0.f, MaxAttribute.CurrentValue);
	Attribute.BaseValue = FMath::Clamp(Attribute.BaseValue, 0.f, MaxAttribute.CurrentValue);
}

void UPFGAttributeSet::ApplyHealthDelta(float Delta)
{
	const float OldValue = Health.CurrentValue;

	Health.CurrentValue += Delta;
	Health.BaseValue += Delta;

	ClampToMax(Health, MaxHealth);

	if (!FMath::IsNearlyEqual(OldValue, Health.CurrentValue))
	{
		OnHealthChanged.Broadcast(OldValue, Health.CurrentValue);
	}
}

void UPFGAttributeSet::SetMaxHealth(float NewMaxHealth)
{
	NewMaxHealth = FMath::Max(0.f, NewMaxHealth);

	MaxHealth.CurrentValue = NewMaxHealth;
	MaxHealth.BaseValue = NewMaxHealth;

	// Max가 줄어들어 CurrentValue가 초과되는 상황을 즉시 재조정
	const float OldHealth = Health.CurrentValue;
	ClampToMax(Health, MaxHealth);

	if (!FMath::IsNearlyEqual(OldHealth, Health.CurrentValue))
	{
		OnHealthChanged.Broadcast(OldHealth, Health.CurrentValue);
	}
}

// 클라이언트 OnRep: 서버가 보낸 값을 그대로 신뢰하고 델리게이트만 브로드캐스트한다.
// 여기서 Clamp나 재계산을 하면 서버와 클라이언트 상태가 분기될 수 있으므로 절대 하지 않는다.
void UPFGAttributeSet::OnRep_Health(FPFGAttributeData OldValue)
{
	OnHealthChanged.Broadcast(OldValue.CurrentValue, Health.CurrentValue);
}

void UPFGAttributeSet::OnRep_MaxHealth(FPFGAttributeData OldValue)
{
	// Max 변경 알림이 필요하면 별도 델리게이트 추가 가능. 여기서는 UI 갱신 트리거로 충분.
}