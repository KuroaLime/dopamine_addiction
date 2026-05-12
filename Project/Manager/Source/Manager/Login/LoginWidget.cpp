// Fill out your copyright notice in the Description page of Project Settings.


#include "Login/LoginWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void ULoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &ULoginWidget::OnLoginButtonClick);
	}

	if(PWInput)
	{
		PWInput->SetIsPassword(true);
	}
}

void ULoginWidget::OnLoginButtonClick()
{
	FString ID = IDInput->GetText().ToString();
	FString PW = PWInput->GetText().ToString();

	if (!ID.IsEmpty())
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName("Lobby_Stage"));
	}
}