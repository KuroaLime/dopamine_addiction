// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Login/UI/LoginWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Math/UnrealMathUtility.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "Game/Login/LoginPlayerController.h"

namespace
{
    constexpr int32 MaxLoginIdLength = 8;
}

void ULoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IDInput)
	{
		IDInput->OnTextChanged.AddDynamic(this, &ULoginWidget::OnIdTextChanged);
	}
	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &ULoginWidget::OnLoginButtonClick);
	}

	if(PWInput)
	{
		PWInput->SetIsPassword(true);
	}

	// 이미지 애니메이션 시작 (무한반복)
	if (ImageAnimation)
	{
		PlayAnimation(ImageAnimation, 0.0f, 0);
	}
}

void ULoginWidget::OnIdTextChanged(const FText& Text)
{
	if (!IDInput)
	{
		return;
	}

	const FString ID = Text.ToString();
	if (ID.Len() > MaxLoginIdLength)
	{
		IDInput->SetText(FText::FromString(ID.Left(MaxLoginIdLength)));
	}
}

void ULoginWidget::OnLoginButtonClick()
{
	FString ID = IDInput->GetText().ToString();
	FString PW = PWInput->GetText().ToString();

	if (!ID.IsEmpty())
	{
		ALoginPlayerController* PC = Cast<ALoginPlayerController>(GetOwningPlayer());

		if(PC)
		{
			PC->ClientLogin(ID, PW);
		}
	}
}