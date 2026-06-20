// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/System/ShopInputHandler.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"

UShopInputHandler::UShopInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UShopInputHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UShopInputHandler::InputActivate()
{
	Super::InputActivate();

	if (!OwnerController) return;
	OwnerController->bShowMouseCursor = true;
	OwnerController->SetInputMode(FInputModeGameAndUI());
}

void UShopInputHandler::InputDeactivate()
{
	Super::InputDeactivate();
	if (!OwnerController) return;

	OwnerController->bShowMouseCursor = false;
	OwnerController->SetInputMode(FInputModeGameOnly());
}

void UShopInputHandler::AddInputMappingContexts()
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

void UShopInputHandler::RemoveInputMappingContexts()
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

void UShopInputHandler::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent) return;

	if (IA_Out)
	{
		EnhancedInputComponent->BindAction(IA_Out, ETriggerEvent::Completed, this, &UShopInputHandler::Input_Out);
	}
}

void UShopInputHandler::Input_Out()
{
	if (UPFGASC* ASC = ResolveOwnerASC())
	{
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));
	}
}