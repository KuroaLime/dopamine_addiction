// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetParent.h"
#include "Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"


void UWidgetParent::BindCharacterState(UCharacterStateComponent* NewCharacterState) {
	
}

void UWidgetParent::NativeConstruct() {
	Super::NativeConstruct();
}

void UWidgetParent::NativeDestruct() {
	Super::NativeDestruct();
}

void UWidgetParent::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) {
	Super::NativeTick(MyGeometry, InDeltaTime);
}