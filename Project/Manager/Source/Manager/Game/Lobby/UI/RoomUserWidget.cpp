// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/UI/RoomUserWidget.h"
#include "Components/TextBlock.h"

void URoomUserWidget::UpdateUserInfo(const FString& UserName, bool bIsReady)
{
	if (UserNameText)
	{
		UserNameText->SetText(FText::FromString(UserName));
	}
	if (UserStateText)
	{
		UserStateText->SetText(bIsReady ? FText::FromString(TEXT("Ready")) : FText::FromString(TEXT("Not Ready")));
		UserStateText->SetColorAndOpacity(bIsReady ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red));
	}
}