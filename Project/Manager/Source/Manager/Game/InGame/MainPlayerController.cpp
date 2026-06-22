// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"

#include "Game/InGame/MainGameState.h"

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
//임시방편
void AMainPlayerController::BeginDestroy()
{
	InputHandlerMap.Empty();
	UIHandlerMap.Empty();
	PhaseStack.Empty();
	CurrentUpgradeOptions.Empty();

	Super::BeginDestroy();
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

	AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (!IsValid(GS)) return;

	UDataTable* ShopTable = GS->GetShopRandomCardDataTable();
	if (!IsValid(ShopTable)) return;

	TArray<FName> RowNames = ShopTable->GetRowNames();
	if (RowNames.Num() == 0)return;

	CurrentUpgradeOptions.Empty();
	TArray<FName> SelectedRowNames;
	TArray<FName> TempRowNames = RowNames;

	for (int32 i = 0; i < 3; ++i)
	{
		if (TempRowNames.Num() == 0) break;

		int32 RandomIdx = FMath::RandRange(0, TempRowNames.Num() - 1);
		SelectedRowNames.Add(TempRowNames[RandomIdx]);
		TempRowNames.RemoveAt(RandomIdx);
	}

	for (const FName& RowName : SelectedRowNames)
	{
		FRandomUpgradeCardDataTable* CardData = ShopTable->FindRow<FRandomUpgradeCardDataTable>(RowName, TEXT("Context_RollUpgrade"));
		if (!CardData) continue;
		FRandomCardOption NewOption;
		NewOption.CardRowName = RowName;

		for (const auto& Pair : CardData->UpgradeValue)
		{
			EUpgradeType StatType = Pair.Key;
			FStatRangeInfo RangeInfo = Pair.Value;

			float RolledValue = FMath::FRandRange(RangeInfo.MinValue, RangeInfo.MaxValue);
			NewOption.RolledStats.Add(StatType, RolledValue);

		}
		CurrentUpgradeOptions.Add(NewOption);
	}
	Client_ReceiveRandomUpgradeOptions(CurrentUpgradeOptions);
}

void AMainPlayerController::Client_ReceiveRandomUpgradeOptions_Implementation(const TArray<FRandomCardOption>& Options)
{

	if (!UIHandlerMap.Contains(EGamePhase::Shop)) return;

	UUIHandler* Handler = UIHandlerMap[EGamePhase::Shop];
	if (!IsValid(Handler)) return;

	// UIHandler는 UUserWidget만 알면 됨, Cast는 Controller에서
	UShopWidget* Shop = Cast<UShopWidget>(Handler->GetWidget());
	if (!IsValid(Shop)) return;

	Shop->Update_UpgradeSelectionWidget(Options);
}

void AMainPlayerController::Server_SelectUpgradeOption_Implementation(int32 SelectedIndex)
{

	if (!CurrentUpgradeOptions.IsValidIndex(SelectedIndex)) return;

	const FRandomCardOption& ChosenOption = CurrentUpgradeOptions[SelectedIndex];

	if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>()) {
		PS->ApplyCardUpgrade(ChosenOption.RolledStats);
	}

	CurrentUpgradeOptions.Empty();
}
void AMainPlayerController::Server_SetUITimer_Implementation(int32 time)
{
	Client_SetUITimer(time);
}

void AMainPlayerController::Client_SetUITimer_Implementation(int32 time)
{
	if (UIHandlerMap.Contains(CurrentPhase))
		UIHandlerMap[CurrentPhase]->SetUITimer(time);
}

bool AMainPlayerController::Server_RequestUpgrade_Validate(int32 ItemID)
{
	return true;
}

void AMainPlayerController::Server_RequestUpgrade_Implementation(int32 ItemID)
{
	if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>())
	{
		PS->Server_ApplyUpgrad_Implementation(static_cast<EUpgradeType>(ItemID));
	}
}

bool AMainPlayerController::Server_RequestPickupCard_Validate(ACardDropActor* TargetCard)
{
    return true;
}

void AMainPlayerController::Server_RequestPickupCard_Implementation(ACardDropActor* TargetCard)
{
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM)
    {
        return;
    }

    GM->TryPickupCard(this, TargetCard);
}


bool AMainPlayerController::Server_RequestPickupNearestCard_Validate()
{
    return true;
}

void AMainPlayerController::Server_RequestPickupNearestCard_Implementation()
{
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM)
    {
        return;
    }

    GM->TryPickupNearestCard(this);
}


bool AMainPlayerController::Server_SubmitSeotdaSelection_Validate(bool bCard0, bool bCard1, bool bCard2)
{
    return true;
}

void AMainPlayerController::Server_SubmitSeotdaSelection_Implementation(bool bCard0, bool bCard1, bool bCard2)
{
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM)
    {
        return;
    }

    GM->SubmitSeotdaSelection(this, bCard0, bCard1, bCard2);
}


bool AMainPlayerController::Server_RequestSeotdaBetAction_Validate(EBettingAction Action)
{
    return true;
}

void AMainPlayerController::Server_RequestSeotdaBetAction_Implementation(EBettingAction Action)
{
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM)
    {
        return;
    }

    GM->SubmitSeotdaBetAction(this, Action);
}