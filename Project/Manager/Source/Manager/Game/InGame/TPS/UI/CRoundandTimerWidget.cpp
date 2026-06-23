// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/CRoundandTimerWidget.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"

void UCRoundandTimerWidget::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS && GS->Implements<UPhaseGameStateInterface>())
	{
		IPhaseGameStateInterface* TimeProvider = Cast<IPhaseGameStateInterface>(GS);
		TimeProvider->GetOnTimeUpdated().AddDynamic(this, &UCRoundandTimerWidget::UpdateTimer_TextImage);
		UpdateTimer_TextImage(TimeProvider->GetRemainingTime());
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
		AGameStateBase* GS = World->GetGameState();
		if (GS && GS->Implements<UPhaseGameStateInterface>())
		{
			Cast<IPhaseGameStateInterface>(GS)->GetOnTimeUpdated().RemoveDynamic(this, &UCRoundandTimerWidget::UpdateTimer_TextImage);
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