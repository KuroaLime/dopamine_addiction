// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoldenGoblinHPWidget.generated.h"

class AGoldenGoblinCharacter;
class UProgressBar;

/**
 * 골든고블린 머리 위 WidgetComponent에 할당하는 체력 바.
 * BP에서 이 클래스를 상속받아 프로그레스바 이름을 "PB_HPBar"로 만들어두면
 * AGoldenGoblinCharacter::BeginPlay()가 자동으로 찾아서 연결해준다 (별도 BP 배선 불필요).
 */
UCLASS()
class MANAGER_API UGoldenGoblinHPWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindGoblin(AGoldenGoblinCharacter* NewGoblin);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void UpdateHPWidget(float CurrentHealth, float MaxHealth, float HealthRatio);

	UPROPERTY()
	UProgressBar* HPProgressBar;

	TWeakObjectPtr<AGoldenGoblinCharacter> BoundGoblin;
};
