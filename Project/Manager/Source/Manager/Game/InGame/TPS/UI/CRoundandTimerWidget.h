// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "CRoundandTimerWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UCRoundandTimerWidget : public UWidgetParent
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;
	UFUNCTION()
	void UpdateTimer_TextImage(int32 NewTime);
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
protected:

	UFUNCTION()
	void UpdateRoundImage();
	UFUNCTION()
	void OnRoundChanged(int32 NewRound);
private:

	UPROPERTY()
	class UTextBlock* Timer_Text;
	UPROPERTY()
	class UImage* Round[4];

	// 현재 몇 번째 라운드인지 표시하는 텍스트.
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Round_Text = nullptr;
};
