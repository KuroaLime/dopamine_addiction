// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/CRoundandTimerWidget.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/MainGameState.h"
#include "Game/InGame/MainPlayerState.h"

namespace
{
	UTexture2D* GetCurrentRoundTexture(int32 Round)
	{
		static TSoftObjectPtr<UTexture2D> RoundTextures[] = {
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_01.Round_01"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_02.Round_02"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_03.Round_03"))),
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/InGame/TPS/Resource/Round/Round_04.Round_04")))
		};

		return Round >= 1 && Round <= UE_ARRAY_COUNT(RoundTextures)
			? RoundTextures[Round - 1].LoadSynchronous()
			: nullptr;
	}

	void UpdateCurrentRankText(UUserWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		UTextBlock* RankText = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Round_Text")));
		APlayerController* OwningPlayer = Widget->GetOwningPlayer();
		AMainPlayerState* LocalPlayerState = OwningPlayer
			? OwningPlayer->GetPlayerState<AMainPlayerState>()
			: nullptr;
		AGameStateBase* GameState = Widget->GetWorld() ? Widget->GetWorld()->GetGameState() : nullptr;
		if (!RankText || !LocalPlayerState || !GameState)
		{
			if (RankText)
			{
				RankText->SetVisibility(ESlateVisibility::HitTestInvisible);
				RankText->SetRenderOpacity(1.0f);
				RankText->SetText(FText::FromString(TEXT("등수 --")));
			}
			return;
		}

		int32 CurrentRank = 1;
		const int32 LocalGold = LocalPlayerState->CurPlayerData.HoldingGold;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const AMainPlayerState* OtherPlayerState = Cast<AMainPlayerState>(PlayerState);
			if (OtherPlayerState && OtherPlayerState->CurPlayerData.HoldingGold > LocalGold)
			{
				++CurrentRank;
			}
		}

		RankText->SetVisibility(ESlateVisibility::HitTestInvisible);
		RankText->SetRenderOpacity(1.0f);
		RankText->SetText(FText::FromString(FString::Printf(TEXT("등수 %d위"), CurrentRank)));
	}
}

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
	UpdateCurrentRankText(this);

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
	UpdateCurrentRankText(this);
}

void UCRoundandTimerWidget::OnRoundChanged(int32 NewRound)
{
	UpdateRoundImage();
}
void UCRoundandTimerWidget::UpdateRoundImage() {
	AMainGameState* GS = Cast<AMainGameState>(GetWorld()->GetGameState());
	if (!GS) return;
	int32 CurrentRound = GS->CurrentRound;

	if (Round[0])
	{
		if (UTexture2D* RoundTexture = GetCurrentRoundTexture(CurrentRound))
		{
			// The Canvas/HorizontalBox slot owns the icon size. Matching the source size
			// here makes the nearly-square icon overflow and appear stretched.
			Round[0]->SetBrushFromTexture(RoundTexture, false);
		}
		Round[0]->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UpdateCurrentRankText(this);

	// Round00은 현재 라운드 아이콘, Round03은 T_UI_TopBar 배경으로 재사용한다.
	// 과거 누적 라운드 점 표시는 새 3칸 TopBar에서 사용하지 않는다.
	for (int32 i = 1; i < 3; ++i)
	{
		if (Round[i])
		{
			Round[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Round[3])
	{
		Round[3]->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
