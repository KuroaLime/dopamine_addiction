// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CRoundandTimerWidget.h"
#include "Data/CharacterStateComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "ManagerGameState.h"

void UCRoundandTimerWidget::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	AManagerGameState* GameState = GetWorld()->GetGameState<AManagerGameState>();
	if (GameState) {
		GameState->OnTimeUpdated.AddDynamic(this, &UCRoundandTimerWidget::UpdateTimer_TextImage);
		UpdateTimer_TextImage(GameState->RemainingTime);
	}
}

void UCRoundandTimerWidget::NativeConstruct() {
	Super::NativeConstruct();

	Timer_Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("Timer_Text")));

	Round[0] = Cast<UImage>(GetWidgetFromName(TEXT("Round00")));
	Round[1] = Cast<UImage>(GetWidgetFromName(TEXT("Round01")));
	Round[2] = Cast<UImage>(GetWidgetFromName(TEXT("Round02")));
	Round[3] = Cast<UImage>(GetWidgetFromName(TEXT("Round03")));
	
	UpdateRoundImage();
}

void UCRoundandTimerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (AManagerGameState* GameState = World->GetGameState<AManagerGameState>())
		{
			GameState->OnTimeUpdated.RemoveDynamic(this, &UCRoundandTimerWidget::UpdateTimer_TextImage);
		}
	}
	Super::NativeDestruct();
}

void UCRoundandTimerWidget::UpdateTimer_TextImage(int32 NewTime) {
	UE_LOG(LogTemp, Warning, TEXT("Timer Text set.."));
	if (nullptr != Timer_Text) Timer_Text->SetText(FText::AsNumber(NewTime));
}

void UCRoundandTimerWidget::UpdateRoundImage() {
	if (CurrentCharacterState.IsValid()) {
		//if (nullptr != GoldBackgroundImage) GoldBackgroundImage->SetBrushFromTexture("ddf");
	}
}