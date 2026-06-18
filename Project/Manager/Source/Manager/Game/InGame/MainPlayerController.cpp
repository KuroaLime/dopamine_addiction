// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Game/InGame/MainPlayerState.h"
//이거 치울방법 찾아보자
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"

void AMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitHandler();
	SetupHandlerInput();

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
	if (HasAuthority())
	{
		Multicast_SwitchMode(NewPhase);
	}
	else
	{
		Server_SwitchMode(NewPhase);
	}
}

void AMainPlayerController::SwitchToLevel(FName LevelToUnload, FName LevelToLoad)
{
	Server_SwitchToLevel(LevelToUnload, LevelToLoad);
}

void AMainPlayerController::SwitchState(EGamePhase NewPhase)
{
	if (HasAuthority())
	{
		Client_SwitchState(NewPhase);
	}
	else
	{
		Server_SwitchState(NewPhase);
	}
}

EGamePhase AMainPlayerController::GetCurrentPhase()
{
	return CurrentPhase;
}

void AMainPlayerController::PushMode(EGamePhase NewPhase)
{
	if (HasAuthority())
		Multicast_PushMode(NewPhase);
	else
		Server_PushMode(NewPhase);
}

void AMainPlayerController::PopMode()
{
	if (HasAuthority())
		Multicast_PopMode();
	else
		Server_PopMode();
}

void AMainPlayerController::SetUITimer(int32 time)
{
	if (HasAuthority())
		Client_SetUITimer(time);
	else
		Server_SetUITimer(time);
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
void AMainPlayerController::Multicast_SwitchMode_Implementation(EGamePhase NewPhase)
{
	ApplySwitchMode(NewPhase);
}

void AMainPlayerController::Server_SwitchMode_Implementation(EGamePhase NewPhase)
{
	// 서버에서 받아서 모든 클라이언트에 전파
	Multicast_SwitchMode(NewPhase);
}

void AMainPlayerController::ApplySwitchMode(EGamePhase NewPhase)
{
	for (auto& Pair : InputHandlerMap)
	{
		if (Pair.Value)
		{
			Pair.Value->InputDeactivate();
		}
	}
	for (auto& Pair : UIHandlerMap)
	{
		if (Pair.Value)
		{
			Pair.Value->UIDeactivate();
		}
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

bool AMainPlayerController::Server_SwitchState_Validate(EGamePhase NewPhase)
{
	return true;
}

void AMainPlayerController::Server_SwitchState_Implementation(EGamePhase NewPhase)
{
	Client_SwitchState(NewPhase);
}

void AMainPlayerController::Client_SwitchState_Implementation(EGamePhase NewPhase)
{
	for (auto& Pair : InputHandlerMap)
	{
		if (Pair.Value)
		{
			Pair.Value->InputDeactivate();
		}
	}
	for (auto& Pair : UIHandlerMap)
	{
		if (Pair.Value)
		{
			Pair.Value->UIDeactivate();
		}
	}

	if (InputHandlerMap.Contains(NewPhase))
	{
		InputHandlerMap[NewPhase]->InputActivate();
	}
	if (UIHandlerMap.Contains(NewPhase))
	{
		UIHandlerMap[NewPhase]->UIActivate();
		UIHandlerMap[NewPhase]->SetIsFocusable(false);
	}
	CurrentPhase = NewPhase;
}

void AMainPlayerController::Server_PushMode_Implementation(EGamePhase NewPhase)
{
	Multicast_PushMode(NewPhase);
}

void AMainPlayerController::Multicast_PushMode_Implementation(EGamePhase NewPhase)
{
	if (InputHandlerMap.Contains(CurrentPhase))
		InputHandlerMap[CurrentPhase]->InputDeactivate();

	PhaseStack.Push(CurrentPhase);

	if (InputHandlerMap.Contains(NewPhase))
		InputHandlerMap[NewPhase]->InputActivate();
	if (UIHandlerMap.Contains(NewPhase))
		UIHandlerMap[NewPhase]->UIActivate();

	CurrentPhase = NewPhase;
}

void AMainPlayerController::Server_PopMode_Implementation()
{
	Multicast_PopMode();
}

void AMainPlayerController::Multicast_PopMode_Implementation()
{
	if (PhaseStack.IsEmpty()) return;

	if (InputHandlerMap.Contains(CurrentPhase))
		InputHandlerMap[CurrentPhase]->InputDeactivate();
	if (UIHandlerMap.Contains(CurrentPhase))
		UIHandlerMap[CurrentPhase]->UIDeactivate();

	EGamePhase PrevPhase = PhaseStack.Pop();

	if (InputHandlerMap.Contains(PrevPhase))
		InputHandlerMap[PrevPhase]->InputActivate();

	CurrentPhase = PrevPhase;
}

void AMainPlayerController::Server_RequestRandomUpgradeOptions_Implementation()
{
	TArray<EUpgradeType> AllTypes;
	for (uint8 i = (uint8)EUpgradeType::Weapon_Damage; i <= (uint8)EUpgradeType::Weapon_Reload;++i) {
		AllTypes.Add(static_cast<EUpgradeType>(i));
	}

	CurrentUpgradeOptions.Empty();
	for (int32 i = 0; i < 3; i++) {
		if (AllTypes.Num() == 0)break;
		int32 RandomIdx = FMath::RandRange(0, AllTypes.Num() - 1);
		CurrentUpgradeOptions.Add(AllTypes[RandomIdx]);
		AllTypes.RemoveAt(RandomIdx);
	}
	Client_ReceiveRandomUpgradeOptions(CurrentUpgradeOptions);
}

void AMainPlayerController::Client_ReceiveRandomUpgradeOptions_Implementation(const TArray<EUpgradeType>& Options)
{
	if (UIHandlerMap.Contains(EGamePhase::TPS))
	{
		UUIHandler * Handler = UIHandlerMap[EGamePhase::TPS];

		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UShopWidget::StaticClass());
		for (UUserWidget* Widget : FoundWidgets){
			UShopWidget * Shop = Cast<UShopWidget>(Widget);
			if (Shop && Shop->GetOwningPlayer() == this){
				Shop->Update_UpgradeSelectionWidget(Options);
				return;
			}
		}
	}
}

void AMainPlayerController::Server_SelectUpgradeOption_Implementation(int32 SelectedIndex)
{
	if (!CurrentUpgradeOptions.IsValidIndex(SelectedIndex)) return;

	EUpgradeType ChosenType = CurrentUpgradeOptions[SelectedIndex];

	if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>()) {
		PS->Server_ApplyUpgrad_Implementation(ChosenType);
	}
	CurrentUpgradeOptions.Empty();

void AMainPlayerController::Server_SetUITimer_Implementation(int32 time)
{
	Client_SetUITimer(time);
}

void AMainPlayerController::Client_SetUITimer_Implementation(int32 time)
{
	if (UIHandlerMap.Contains(CurrentPhase))
		UIHandlerMap[CurrentPhase]->SetUITimer(time);
}


