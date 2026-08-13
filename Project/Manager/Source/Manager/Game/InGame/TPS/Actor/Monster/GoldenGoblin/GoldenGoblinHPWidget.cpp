// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinHPWidget.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "Components/ProgressBar.h"

void UGoldenGoblinHPWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_HPBar")));
}

void UGoldenGoblinHPWidget::BindGoblin(AGoldenGoblinCharacter* NewGoblin)
{
	if (!NewGoblin)
	{
		return;
	}

	BoundGoblin = NewGoblin;
	NewGoblin->OnGoblinHealthChanged.AddDynamic(this, &UGoldenGoblinHPWidget::UpdateHPWidget);

	if (HPProgressBar)
	{
		HPProgressBar->SetPercent(NewGoblin->GetHealthRatio());
	}
}

void UGoldenGoblinHPWidget::UpdateHPWidget(float CurrentHealth, float MaxHealth, float HealthRatio)
{
	if (HPProgressBar)
	{
		HPProgressBar->SetPercent(HealthRatio);
	}
}
