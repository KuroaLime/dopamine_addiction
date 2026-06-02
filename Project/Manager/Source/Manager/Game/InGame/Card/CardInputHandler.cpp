// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/CardInputHandler.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"

UCardInputHandler::UCardInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UCardInputHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UCardInputHandler::InputActivate()
{
	Super::InputActivate();

	if (!OwnerController) return;
	OwnerController->bShowMouseCursor = true;
	OwnerController->SetInputMode(FInputModeGameAndUI());
}

void UCardInputHandler::InputDeactivate()
{
	Super::InputDeactivate();
}

void UCardInputHandler::AddInputMappingContexts()
{
	for (UInputMappingContext* Context : MappingContexts)
	{
		AddMappingContext(Context, 0);
	}

	if (MappingContext)
	{
		AddMappingContext(MappingContext, 0);
	}
}

void UCardInputHandler::RemoveInputMappingContexts()
{
	for (UInputMappingContext* Context : MappingContexts)
	{
		RemoveMappingContext(Context);
	}

	if (MappingContext)
	{
		RemoveMappingContext(MappingContext);
	}
}

void UCardInputHandler::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent) return;

	if (IA_Check)           EnhancedInputComponent->BindAction(IA_Check, ETriggerEvent::Started, this, &UCardInputHandler::Input_Check);
	if (IA_Call)            EnhancedInputComponent->BindAction(IA_Call, ETriggerEvent::Started, this, &UCardInputHandler::Input_Call);
	if (IA_Half)            EnhancedInputComponent->BindAction(IA_Half, ETriggerEvent::Started, this, &UCardInputHandler::Input_Half);
	if (IA_Die)             EnhancedInputComponent->BindAction(IA_Die, ETriggerEvent::Started, this, &UCardInputHandler::Input_Die);
	if (IA_AllIn)           EnhancedInputComponent->BindAction(IA_AllIn, ETriggerEvent::Started, this, &UCardInputHandler::Input_AllIn);
	if (IA_SelectCard1)     EnhancedInputComponent->BindAction(IA_SelectCard1, ETriggerEvent::Started, this, &UCardInputHandler::Input_SelectCard1);
	if (IA_SelectCard2)     EnhancedInputComponent->BindAction(IA_SelectCard2, ETriggerEvent::Started, this, &UCardInputHandler::Input_SelectCard2);
	if (IA_SelectCard3)     EnhancedInputComponent->BindAction(IA_SelectCard3, ETriggerEvent::Started, this, &UCardInputHandler::Input_SelectCard3);
	if (IA_ConfirmSelection) EnhancedInputComponent->BindAction(IA_ConfirmSelection, ETriggerEvent::Started, this, &UCardInputHandler::Input_ConfirmSelection);
}

void UCardInputHandler::Input_Check()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[Card] Check"));
}

void UCardInputHandler::Input_Call()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[Card] Call"));
}

void UCardInputHandler::Input_Half()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[Card] Half"));
}

void UCardInputHandler::Input_Die()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Card] Die"));
}

void UCardInputHandler::Input_AllIn()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("[Card] AllIn"));
}

void UCardInputHandler::Input_SelectCard1()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[Card] SelectCard1"));
}

void UCardInputHandler::Input_SelectCard2()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[Card] SelectCard2"));
}

void UCardInputHandler::Input_SelectCard3()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[Card] SelectCard3"));
}

void UCardInputHandler::Input_ConfirmSelection()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[Card] ConfirmSelection"));
}