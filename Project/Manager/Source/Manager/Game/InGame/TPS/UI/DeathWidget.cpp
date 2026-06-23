// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/DeathWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "Animation/WidgetAnimation.h"

const FName UDeathWidget::RadialWipeParamName(TEXT("Radial_wipe"));

void UDeathWidget::NativeConstruct()
{
    Super::NativeConstruct();

    DisplayTime = 0.f;
    TargetTime = 0.f;
    PrevTargetTime = 0.f;
    InterpElapsed = 0.f;
    MaxTime = 0.f;

    if (RespawnCircle)
    {
        UMaterialInterface* BaseMaterial = RespawnCircle->GetDynamicMaterial();
        RespawnCircleMID = Cast<UMaterialInstanceDynamic>(BaseMaterial);

        if (RespawnCircleMID)
        {
            RespawnCircleMID->SetScalarParameterValue(RadialWipeParamName, 0.f);
        }
    }
    PlayLoopingAnimations();
}

void UDeathWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (ServerTickInterval <= 0.f || FMath::IsNearlyEqual(PrevTargetTime, TargetTime))
    {
        DisplayTime = TargetTime;
    }
    else
    {
        InterpElapsed += InDeltaTime;

        const float Alpha = FMath::Clamp(InterpElapsed / ServerTickInterval, 0.f, 1.f);
        DisplayTime = FMath::Lerp(PrevTargetTime, TargetTime, Alpha);
    }

    UpdateCircle(DisplayTime);
}

void UDeathWidget::UpdateTime(int32 time) const
{
    if (!ResponeTimeText) return;

    const float NewTime = static_cast<float>(time);

    const bool bIsNewCycle = (MaxTime <= 0.f) || (NewTime > 0.f && FMath::IsNearlyZero(DisplayTime, 0.01f));

    if (bIsNewCycle)
    {
        MaxTime = NewTime;
        DisplayTime = NewTime;
    }

    const float NewGoal = FMath::Max(NewTime - 1.f, 0.f);

    PrevTargetTime = DisplayTime;
    TargetTime = NewGoal;
    InterpElapsed = 0.f;

    FString TimeStr = FString::Printf(TEXT("%d"), time);
    ResponeTimeText->SetText(FText::FromString(TimeStr));
}

void UDeathWidget::SetRandomCardFrontImage()
{
    if (!CardFrontImage) return;
    if (CardFrontImages.Num() == 0) return;

    const int32 RandIndex = FMath::RandRange(0, CardFrontImages.Num() - 1);
    CardFrontImage->SetBrushFromTexture(CardFrontImages[RandIndex]);
}

void UDeathWidget::UpdateCircle(float CurrentTime) const
{
    if (!RespawnCircleMID) return;
    if (MaxTime <= 0.f) return;

    const float RemainPercent = FMath::Clamp(CurrentTime / MaxTime, 0.f, 1.f);
    const float ElapsedPercent = 1.f - RemainPercent;

    RespawnCircleMID->SetScalarParameterValue(RadialWipeParamName, ElapsedPercent);
}

void UDeathWidget::PlayLoopingAnimations() const
{
    UDeathWidget* MutableThis = const_cast<UDeathWidget*>(this);

    if (CardRotateAnim)
    {
        MutableThis->PlayAnimation(CardRotateAnim, 0.f, 0, EUMGSequencePlayMode::Forward, CardAnimPlayRate);
    }

    if (PetalAnim)
    {
        MutableThis->PlayAnimation(PetalAnim, 0.f, 0, EUMGSequencePlayMode::Forward, PetalAnimPlayRate);
    }
}