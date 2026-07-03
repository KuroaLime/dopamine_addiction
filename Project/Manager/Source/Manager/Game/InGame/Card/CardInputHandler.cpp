// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Card/CardInputHandler.h"
#include "Manager.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"

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
    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController))
    {
        MainPC->Server_RequestSeotdaBetAction(EBettingAction::Check);
    }
    DS_SCREEN(-1, 2.0f, FColor::Cyan, TEXT("[Card] Check"));
}

void UCardInputHandler::Input_Call()
{
    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController))
    {
        MainPC->Server_RequestSeotdaBetAction(EBettingAction::Call);
    }
    DS_SCREEN(-1, 2.0f, FColor::Cyan, TEXT("[Card] Call"));
}

void UCardInputHandler::Input_Half()
{
    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController))
    {
        MainPC->Server_RequestSeotdaBetAction(EBettingAction::Half);
    }
    DS_SCREEN(-1, 2.0f, FColor::Cyan, TEXT("[Card] Half"));
}

void UCardInputHandler::Input_Die()
{
    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController))
    {
        MainPC->Server_RequestSeotdaBetAction(EBettingAction::Die);
    }
    DS_SCREEN(-1, 2.0f, FColor::Red, TEXT("[Card] Die"));
}

void UCardInputHandler::Input_AllIn()
{
    if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController))
    {
        MainPC->Server_RequestSeotdaBetAction(EBettingAction::AllIn);
    }
    DS_SCREEN(-1, 2.0f, FColor::Magenta, TEXT("[Card] AllIn"));
}

void UCardInputHandler::Input_SelectCard1()
{
    bSelectedCard0 = !bSelectedCard0;
    if (GEngine)
    {
        DS_SCREEN(-1, 2.0f, bSelectedCard0 ? FColor::Green : FColor::Yellow,
            FString::Printf(TEXT("[Card] SelectCard1=%d"), bSelectedCard0 ? 1 : 0));
    }
}

void UCardInputHandler::Input_SelectCard2()
{
    bSelectedCard1 = !bSelectedCard1;
    if (GEngine)
    {
        DS_SCREEN(-1, 2.0f, bSelectedCard1 ? FColor::Green : FColor::Yellow,
            FString::Printf(TEXT("[Card] SelectCard2=%d"), bSelectedCard1 ? 1 : 0));
    }
}

void UCardInputHandler::Input_SelectCard3()
{
    bSelectedCard2 = !bSelectedCard2;
    if (GEngine)
    {
        DS_SCREEN(-1, 2.0f, bSelectedCard2 ? FColor::Green : FColor::Yellow,
            FString::Printf(TEXT("[Card] SelectCard3=%d"), bSelectedCard2 ? 1 : 0));
    }
}

void UCardInputHandler::Input_ConfirmSelection()
{
    AMainPlayerController* MainPC = Cast<AMainPlayerController>(OwnerController);
    if (!MainPC)
    {
        return;
    }

    MainPC->Server_SubmitSeotdaSelection(bSelectedCard0, bSelectedCard1, bSelectedCard2);

    if (GEngine)
    {
        DS_SCREEN(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("[Card] Submit selection %d/%d/%d"),
                bSelectedCard0 ? 1 : 0,
                bSelectedCard1 ? 1 : 0,
                bSelectedCard2 ? 1 : 0));
    }
}
