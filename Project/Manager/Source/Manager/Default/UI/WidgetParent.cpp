// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/UI/WidgetParent.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"


void UWidgetParent::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

}

void UWidgetParent::OnPlayerStateReady() {

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