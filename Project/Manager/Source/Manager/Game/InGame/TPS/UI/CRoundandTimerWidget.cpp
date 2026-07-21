// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/CRoundandTimerWidget.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/MainGameState.h"

void UCRoundandTimerWidget::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS && GS->Implements<UPhaseGameStateInterface>())
	{
		IPhaseGameStateInterface* TimeProvider = Cast<IPhaseGameStateInterface>(GS);
		TimeProvider->GetOnTimeUpdated().AddDynamic(this, &UCRoundandTimerWidget::UpdateTimer_TextImage);
		UpdateTimer_TextImage(TimeProvider->GetRemainingTime());

		TimeProvider->GetOnRoundChanged().AddDynamic(this, &UCRoundandTimerWidget::OnRoundChanged);
		UpdateRoundImage();
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
			Cast<IPhaseGameStateInterface>(GS)->GetOnRoundChanged().RemoveDynamic(this, &UCRoundandTimerWidget::OnRoundChanged);
		}
	}
	Super::NativeDestruct();
}

void UCRoundandTimerWidget::UpdateTimer_TextImage(int32 NewTime) {
	if (nullptr != Timer_Text)
	{
		const int32 ClampedTime = FMath::Max(NewTime, 0);
		const int32 Minutes = ClampedTime / 60;
		const int32 Seconds = ClampedTime % 60;
		Timer_Text->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
	}
}

void UCRoundandTimerWidget::OnRoundChanged(int32 NewRound)
{
	UpdateRoundImage();
}
void UCRoundandTimerWidget::UpdateRoundImage() {
	AMainGameState* GS = Cast<AMainGameState>(GetWorld()->GetGameState());
	if (!GS) return;
	int32 CurrentRound = GS->CurrentRound;

	if (Round_Text)
	{
		Round_Text->SetText(FText::FromString(FString::Printf(TEXT("Round %d"), CurrentRound)));
	}

	for (int32 i = 0; i < 4; ++i)
	{
		if (Round[i])
		{
			if (i < CurrentRound)
			{
				Round[i]->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				Round[i]->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
}