// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/Handler/InputHandler.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

UInputHandler::UInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UInputHandler::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = Cast<APlayerController>(GetOwner());
}

void UInputHandler::InputActivate()
{
	if (!OwnerController) return;

	AddInputMappingContexts();
}

void UInputHandler::InputDeactivate()
{
	if (!OwnerController) return;

	RemoveInputMappingContexts();	
}

void UInputHandler::AddMappingContext(UInputMappingContext* Context, int32 Priority)
{
	if (!Context || !OwnerController) return;

	if (ULocalPlayer* LocalPlayer = OwnerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->AddMappingContext(Context, Priority);
		}
	}
}

void UInputHandler::RemoveMappingContext(UInputMappingContext* Context)
{
	if (!Context || !OwnerController) return;

	if (ULocalPlayer* LocalPlayer = OwnerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->RemoveMappingContext(Context);
		}
	}
}

void UInputHandler::ClearAllMappingContexts()
{
	if (!OwnerController) return;

	if (ULocalPlayer* LocalPlayer = OwnerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->ClearAllMappings();
		}
	}
}

UPFGASC* UInputHandler::ResolveOwnerASC() const
{
	if (!OwnerController) return nullptr;

	APawn* OwnerPawn = OwnerController->GetPawn();
	if (!OwnerPawn) return nullptr;

	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OwnerPawn);
	if (!ASCInterface) return nullptr;

	return ASCInterface->GetASC();
}