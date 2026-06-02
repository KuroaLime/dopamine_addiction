// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Kismet/GameplayStatics.h"


void AMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitHandler();
	SetupHandlerInput();

	CurrentPhase = EGamePhase::TPS;
	SwitchMode(CurrentPhase);
}

void AMainPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AMainPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AMainPlayerController::SwitchMode(EGamePhase NewPhase)
{
	if (InputHandlerMap.Contains(CurrentPhase))
	{
		InputHandlerMap[CurrentPhase]->InputDeactivate();
	}
	if (UIHandlerMap.Contains(CurrentPhase))
	{
		UIHandlerMap[CurrentPhase]->UIDeactivate();
	}

	if (InputHandlerMap.Contains(NewPhase))
	{
		InputHandlerMap[NewPhase]->InputActivate();
	}
	if (UIHandlerMap.Contains(NewPhase))
	{
		UIHandlerMap[NewPhase]->UIActivate();
	}

	CurrentPhase = NewPhase;
}

void AMainPlayerController::SwitchToLevel(FName LevelToUnload, FName LevelToLoad)
{
	Server_SwitchToLevel(LevelToUnload, LevelToLoad);
}

void AMainPlayerController::InitHandler()
{
	for (auto& Pair : InputHandlerClassMap)
	{
		if (!Pair.Value) continue;

		UInputHandler* Handler = NewObject<UInputHandler>(this, Pair.Value);
		if (Handler)
		{
			Handler->RegisterComponent();
			InputHandlerMap.Add(Pair.Key, Handler);
		}
	}

	for (auto& Pair : UIHandlerClassMap)
	{
		if (!Pair.Value) continue;

		UUIHandler* Handler = NewObject<UUIHandler>(this, Pair.Value);
		if (Handler)
		{
			Handler->RegisterComponent();
			UIHandlerMap.Add(Pair.Key, Handler);
		}
	}
}

void AMainPlayerController::SetupHandlerInput()
{
	if (!IsLocalPlayerController()) return;

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent) return;

	for (auto& Pair : InputHandlerMap)
	{
		if (Pair.Value)
		{
			Pair.Value->SetupInput(EnhancedInputComponent);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Networked Level Streaming
bool AMainPlayerController::Server_SwitchToLevel_Validate(FName LevelToUnload, FName LevelToLoad)
{
	return true;
}

void AMainPlayerController::Server_SwitchToLevel_Implementation(FName LevelToUnload, FName LevelToLoad)
{
	Client_SwitchToLevel(LevelToUnload, LevelToLoad);
}

void AMainPlayerController::Client_SwitchToLevel_Implementation(FName LevelToUnload, FName LevelToLoad)
{
	if (!LevelToUnload.IsNone())
	{
		FLatentActionInfo UnloadInfo(1, 1, TEXT(""), this);
		UGameplayStatics::UnloadStreamLevel(GetWorld(), LevelToUnload, UnloadInfo, false);
	}

	if (!LevelToLoad.IsNone())
	{
		FLatentActionInfo LoadInfo(2, 2, TEXT(""), this);
		UGameplayStatics::LoadStreamLevel(GetWorld(), LevelToLoad, true, false, LoadInfo);
	}
}