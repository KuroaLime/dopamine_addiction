// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Handler/UIHandler.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Default/UI/WidgetParent.h"

UUIHandler::UUIHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UUIHandler::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController) return;

	CreateHUD();
}

void UUIHandler::UIActivate()
{
	ShowHUD();
}

void UUIHandler::UIDeactivate()
{
	HideHUD();
}

void UUIHandler::UIToggle()
{
	FString NetMode = (GetNetMode() == NM_Client) ? TEXT("Client") : TEXT("Server");
	UE_LOG(LogTemp, Warning, TEXT("[%s] Toggle Called!"), *NetMode);
	


	if (PlayerWidget->GetVisibility() == ESlateVisibility::Visible) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Deactivate"));
		UIDeactivate();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIActivate"));
		UIActivate();
	}
}

void UUIHandler::SetIsFocusable(bool isFocus)
{
	PlayerWidget->SetIsFocusable(isFocus);
}

void UUIHandler::CreateHUD()
{
	if (!OwnerController) return;

	if (!OwnerController->IsLocalPlayerController()) return;

	if (!PlayerUI) return;

	PlayerWidget = CreateWidget<UUserWidget>(OwnerController, PlayerUI);
	if (PlayerWidget)
	{
		PlayerWidget->AddToViewport();
		PlayerWidget->SetVisibility(ESlateVisibility::Collapsed);

		if (UWidgetParent* WidgetParent = Cast<UWidgetParent>(PlayerWidget))
		{
			if (UCharacterStateComponent* State = ResolveOwnerCharacterState())
			{
				WidgetParent->BindCharacterState(State);
			}
		}
	}
}

void UUIHandler::NotifyPlayerStateReady()
{
	if (UWidgetParent* WidgetParent = Cast<UWidgetParent>(PlayerWidget))
	{
		WidgetParent->OnPlayerStateReady();
	}
}

void UUIHandler::ShowHUD()
{
	SetWidgetVisibility(ESlateVisibility::Visible);
}

void UUIHandler::HideHUD()
{
	SetWidgetVisibility(ESlateVisibility::Collapsed);
}

void UUIHandler::SetUITimer(int32 time)
{

}

void UUIHandler::SetWidgetVisibility(ESlateVisibility Visibility)
{
	if (PlayerWidget)
		PlayerWidget->SetVisibility(Visibility);
}

UCharacterStateComponent* UUIHandler::ResolveOwnerCharacterState() const
{
	if (!OwnerController) return nullptr;

	APawn* OwnerPawn = OwnerController->GetPawn();
	if (!OwnerPawn) return nullptr;

	IAbilityOwnerInterface* OwnerInterface = Cast<IAbilityOwnerInterface>(OwnerPawn);
	if (!OwnerInterface) return nullptr;

	return OwnerInterface->GetCharacterState();
}