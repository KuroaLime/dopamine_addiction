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


	void UpdateRoundImage();

private:

	//배경
	UPROPERTY()
	class UTextBlock* Timer_Text;
	//플레이어 아이콘
	UPROPERTY()
	class UImage* Round[4];
};
