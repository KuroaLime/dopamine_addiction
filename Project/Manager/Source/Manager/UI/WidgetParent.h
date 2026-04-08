// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetParent.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UWidgetParent : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void StaticUI() PURE_VIRTUAL(UWidgetParent::StaticUI, );

	TWeakObjectPtr<class UCharacterStateComponent> CurrentCharacterState;
};
