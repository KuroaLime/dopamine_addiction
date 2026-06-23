// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerController.h"
#include "Engine/Engine.h"
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
			UE_LOG(LogTemp, Warning, TEXT("[%s] UIHandlerInit Phase=%d Class=%s Obj=%s Local=%d"),
				HasAuthority() ? TEXT("SV") : TEXT("CL"),
				static_cast<int32>(Pair.Key),
				*GetNameSafe(Handler->GetClass()),
				*GetNameSafe(Handler),
				IsLocalPlayerController() ? 1 : 0);
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

void AMainPlayerController::PickupNearestCard()
{
	Server_RequestPickupNearestCard();
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
	const bool bHasUIForPhase = UIHandlerMap.Contains(NewPhase);
	const bool bHasInputForPhase = InputHandlerMap.Contains(NewPhase);

	UE_LOG(LogTemp, Warning, TEXT("[%s] ApplySwitchMode NewPhase=%d HasUI=%d HasInput=%d UIHandlers=%d InputHandlers=%d Local=%d"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		static_cast<int32>(NewPhase),
		bHasUIForPhase ? 1 : 0,
		bHasInputForPhase ? 1 : 0,
		UIHandlerMap.Num(),
		InputHandlerMap.Num(),
		IsLocalPlayerController() ? 1 : 0);

	if (GEngine && IsLocalPlayerController())
	{
		GEngine->AddOnScreenDebugMessage(
			2026062402,
			5.0f,
			FColor::Yellow,
			FString::Printf(TEXT("[DEBUG] ApplySwitchMode phase=%d HasUI=%d"), static_cast<int32>(NewPhase), bHasUIForPhase ? 1 : 0)
		);
	}
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
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Rand Status UP")));

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
EUpgradeType AMainPlayerController::GetStaticUpgradeTypeFromIndex(int32 Index)
{
	// UI에 고정 능력치 상승 버튼들이 배치된 순서대로 대응시킵니다.
	switch (Index)
	{
	case 0:
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_Health")));

		return EUpgradeType::Player_Health;
	case 1:
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_MoveSpeed")));

		return EUpgradeType::Player_MoveSpeed;
	case 2:
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_HealthRegeneration")));

		return EUpgradeType::Player_HealthRegeneration;
	default:
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("None")));

		return EUpgradeType::None;
	}
}
int32 AMainPlayerController::GetStaticUpgradeCost(EUpgradeType Type, int32 CurrentLevel)
{

	int32 BaseCost = 100;
	return BaseCost + (CurrentLevel * 50);
}
// 현재 레벨을 조회하는 헬퍼
int32 AMainPlayerController::GetCurrentUpgradeLevel(AMainPlayerState* PS, EUpgradeType Type)
{
	if (!PS) return 0;

	switch (Type)
	{
	case EUpgradeType::Player_Health: return PS->PlayerData.LvHealth;
	case EUpgradeType::Player_MoveSpeed: return PS->PlayerData.LvMovementSpeed;
	case EUpgradeType::Player_HealthRegeneration: return PS->PlayerData.LvHealthRegeneration;
	default: return 0;
	}
}
void AMainPlayerController::Server_SelectStaticUpgradeOption_Implementation(int32 SelectedIndex) {

	if (CurrentPhase != EGamePhase::Shop)
	{
		return;
	}
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	EUpgradeType UpgradeType = GetStaticUpgradeTypeFromIndex(SelectedIndex);
	if (UpgradeType == EUpgradeType::None)
		return;

	int32 CurrentLevel = GetCurrentUpgradeLevel(PS, UpgradeType);
	constexpr int32 MaxUpgradeLevel = 5;
	if (CurrentLevel >= MaxUpgradeLevel)
	{
		return;
	}

	/*int32 Cost = GetStaticUpgradeCost(UpgradeType, CurrentLevel);
	if (PS->CurPlayerData.HoldingGold < Cost)
	{
		return;
	}*/

	PS->AddGold(-5);
	PS->Server_ApplyUpgrad_Implementation(UpgradeType);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("static Status UP")));

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

/////////////////////////////////////////////////////////////////////////////////////////
// Pick Up Card
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
void AMainPlayerController::Client_ShowSeotdaResult_Implementation(const FString& ResultText)
{
UE_LOG(LogTemp, Warning, TEXT("[CL] Seotda Result: %s"), *ResultText);

    SeotdaUiLastResultText = ResultText;
    bSeotdaUiMatchEnded = ResultText.Contains(TEXT("[MATCH END]"));

if (GEngine)
{
GEngine->AddOnScreenDebugMessage(
2026062501,
8.0f,
FColor::Green,
ResultText
);
}
}

void AMainPlayerController::Client_UpdateSeotdaState_Implementation(
int32 Round,
bool bBettingActive,
const FString& CurrentTurnPlayerName,
int32 Pot,
int32 CurrentBet,
int32 MyBetMoney,
int32 NeedCall,
bool bMyTurn,
bool bMySubmitted,
bool bMyFolded,
bool bRoundResolved
)
{
SeotdaUiRound = Round;
bSeotdaUiBettingActive = bBettingActive;
SeotdaUiCurrentTurnPlayerName = CurrentTurnPlayerName;
SeotdaUiPot = Pot;
SeotdaUiCurrentBet = CurrentBet;
SeotdaUiMyBetMoney = MyBetMoney;
SeotdaUiNeedCall = NeedCall;
bSeotdaUiMyTurn = bMyTurn;
bSeotdaUiMySubmitted = bMySubmitted;
bSeotdaUiMyFolded = bMyFolded;
bSeotdaUiRoundResolved = bRoundResolved;

UE_LOG(LogTemp, Warning,
TEXT("[CL] SeotdaState Round=%d Betting=%d Turn=%s Pot=%d CurrentBet=%d MyBet=%d NeedCall=%d MyTurn=%d Submitted=%d Folded=%d Resolved=%d"),
SeotdaUiRound,
bSeotdaUiBettingActive ? 1 : 0,
*SeotdaUiCurrentTurnPlayerName,
SeotdaUiPot,
SeotdaUiCurrentBet,
SeotdaUiMyBetMoney,
SeotdaUiNeedCall,
bSeotdaUiMyTurn ? 1 : 0,
bSeotdaUiMySubmitted ? 1 : 0,
bSeotdaUiMyFolded ? 1 : 0,
bSeotdaUiRoundResolved ? 1 : 0
);
}

void AMainPlayerController::ReturnToLobbyFromMatchEnd()
{
    if (!IsLocalController())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[CL] ReturnToLobbyFromMatchEnd"));

    bSeotdaUiMatchEnded = false;
    SeotdaUiLastResultText.Empty();

    ConsoleCommand(TEXT("open /Game/Lobby/Lobby_Stage"));
}
