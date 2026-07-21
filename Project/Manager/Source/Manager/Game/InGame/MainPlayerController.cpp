// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainPlayerController.h"
#include "Game/InGame/MainGameMode.h"
#include "Manager.h"
#include "Algo/Unique.h"
#include "Engine/Engine.h"
#include "EnhancedInputComponent.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/Handler/InputHandler.h"
#include "Game/InGame/Interface/InterfaceInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Default/System/UManagerGameInstance.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/Card/CardGameService.h"
#include "Game/InGame/Card/Actor/CardDropActor.h"
#include "EngineUtils.h"
#include "Engine/LevelStreaming.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Game/InGame/TPS/UI/Shop/ShopWidget.h"
#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

#include "Game/InGame/MainGameState.h"

namespace
{
	constexpr double CardPickupMinimumIntervalSeconds = 0.10;
	constexpr double CardDiscardMinimumIntervalSeconds = 0.15;
	constexpr double SeotdaRevealMinimumIntervalSeconds = 0.25;
	constexpr double SeotdaSelectionMinimumIntervalSeconds = 0.25;
	constexpr double SeotdaBetMinimumIntervalSeconds = 0.25;
	constexpr double StreamLevelAckMinimumIntervalSeconds = 0.10;
	constexpr double CardBundleAckMinimumIntervalSeconds = 0.10;
	constexpr double ShopModeMinimumIntervalSeconds = 0.10;
	constexpr double ShopRandomRollMinimumIntervalSeconds = 0.50;
	constexpr double ShopUpgradeSelectionMinimumIntervalSeconds = 0.10;
	constexpr double ServerRpcRejectedWindowSeconds = 2.0;
	constexpr double ServerRpcWarningCooldownSeconds = 10.0;
	constexpr int32 ServerRpcRejectedWarningThreshold = 10;
	constexpr float ServerPositionCorrectionRetryIntervalSeconds = 0.25f;
	constexpr int32 ServerPositionCorrectionMaxSends = 3;
	constexpr float ServerPositionCorrectionDriftWarningDistance = 50.0f;
	constexpr float ServerPositionCorrectionDriftWarningDegrees = 15.0f;

	float GetControllerRotationErrorDegrees(const FRotator& Left, const FRotator& Right)
	{
		const FRotator Delta = (Left - Right).GetNormalized();
		return FMath::Max3(
			FMath::Abs(Delta.Pitch),
			FMath::Abs(Delta.Yaw),
			FMath::Abs(Delta.Roll));
	}

	const FName& GetControllerPersistentMainWorldLevelName()
	{
		static const FName LevelName(TEXT("Main_Game_World"));
		return LevelName;
	}

	bool IsControllerPersistentMainWorldTarget(FName LevelName)
	{
		return LevelName == GetControllerPersistentMainWorldLevelName();
	}
}

bool AMainPlayerController::TryConsumeServerRpcRateLimit(
	FServerRpcRateLimitState& State,
	double MinimumIntervalSeconds,
	const TCHAR* RpcName)
{
	if (!HasAuthority())
	{
		return false;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	if (!State.bHasAcceptedRequest ||
		NowSeconds - State.LastAcceptedSeconds >= MinimumIntervalSeconds)
	{
		State.LastAcceptedSeconds = NowSeconds;
		State.bHasAcceptedRequest = true;
		return true;
	}

	if (State.RejectedWindowStartSeconds <= 0.0 ||
		NowSeconds - State.RejectedWindowStartSeconds >= ServerRpcRejectedWindowSeconds)
	{
		State.RejectedWindowStartSeconds = NowSeconds;
		State.RejectedInWindow = 0;
	}

	++State.RejectedInWindow;
	if (State.RejectedInWindow >= ServerRpcRejectedWarningThreshold &&
		(State.LastWarningSeconds <= 0.0 ||
			NowSeconds - State.LastWarningSeconds >= ServerRpcWarningCooldownSeconds))
	{
		const FString PlayerLabel = PlayerState ? PlayerState->GetPlayerName() : GetName();
		UE_LOG(LogManager, Warning,
			TEXT("[DS][Security] RpcRateLimited Player=%s Controller=%s Rpc=%s Rejected=%d Window=%.1fs MinIntervalMs=%.0f"),
			*PlayerLabel,
			*GetName(),
			RpcName,
			State.RejectedInWindow,
			ServerRpcRejectedWindowSeconds,
			MinimumIntervalSeconds * 1000.0);
		State.LastWarningSeconds = NowSeconds;
	}

	return false;
}

bool AMainPlayerController::TryConsumeCardPickupRequest()
{
	return TryConsumeServerRpcRateLimit(
		CardPickupRateLimitState,
		CardPickupMinimumIntervalSeconds,
		TEXT("CardPickup"));
}

void AMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitHandler();
	SetupHandlerInput();

	ApplySwitchMode(CurrentPhase);
}

void AMainPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		UWidgetLayoutLibrary::RemoveAllWidgets(this);
	}

	ClearPendingServerPositionCorrection();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
		World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	}

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
		ApplySwitchMode(NewPhase);
	}
}

void AMainPlayerController::SwitchToLevel(FName LevelToUnload, FName LevelToLoad)
{
	if (HasAuthority())
	{
		Client_SwitchToLevel(LevelToUnload, LevelToLoad);
	}
}

void AMainPlayerController::SwitchState(EGamePhase NewPhase)
{
	if (HasAuthority())
	{
		Client_SwitchState(NewPhase);
	}
	else
	{
		Client_SwitchState_Implementation(NewPhase);
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
		Client_SetUITimer_Implementation(time);
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
			UE_LOG(LogTemp, Verbose, TEXT("[%s] UIHandlerInit Phase=%d Class=%s Obj=%s Local=%d"),
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Networked Level Streaming
void AMainPlayerController::Multicast_SwitchMode_Implementation(EGamePhase NewPhase)
{
	ApplySwitchMode(NewPhase);
}

bool AMainPlayerController::Server_SwitchMode_Validate(EGamePhase NewPhase)
{
	return false;
}

void AMainPlayerController::Server_SwitchMode_Implementation(EGamePhase NewPhase)
{
	UE_LOG(LogTemp, Warning, TEXT("[DS] Rejected client phase switch request Phase=%d Player=%s"),
		static_cast<int32>(NewPhase),
		*GetNameSafe(PlayerState));
}

void AMainPlayerController::ApplySwitchMode(EGamePhase NewPhase)
{
	const bool bHasUIForPhase = UIHandlerMap.Contains(NewPhase);
	const bool bHasInputForPhase = InputHandlerMap.Contains(NewPhase);

	UE_LOG(LogTemp, Verbose, TEXT("[%s] ApplySwitchMode NewPhase=%d HasUI=%d HasInput=%d UIHandlers=%d InputHandlers=%d Local=%d"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		static_cast<int32>(NewPhase),
		bHasUIForPhase ? 1 : 0,
		bHasInputForPhase ? 1 : 0,
		UIHandlerMap.Num(),
		InputHandlerMap.Num(),
		IsLocalPlayerController() ? 1 : 0);

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

void AMainPlayerController::SetGameplayInputLocked(bool bLocked, const TCHAR* Context)
{
	ApplyGameplayInputLock(bLocked, Context);

	if (HasAuthority())
	{
		if (!bLocked && bPendingServerPositionCorrection)
		{
			UE_LOG(LogManager, Display,
				TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
				*GetNameSafe(PlayerState),
				PendingServerPositionCorrectionSendCount,
				*PendingServerPositionCorrectionContext);
			ClearPendingServerPositionCorrection();
		}

		Client_SetGameplayInputLocked(bLocked, FString(Context ? Context : TEXT("<NULL>")));
	}
}

void AMainPlayerController::StartServerAuthoritativePositionCorrection(
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	const TCHAR* Context)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionSkipped Reason=MissingPawn Player=%s Context=%s"),
			*GetNameSafe(PlayerState),
			Context ? Context : TEXT("<NULL>"));
		return;
	}

	if (TargetLocation.ContainsNaN() || TargetRotation.ContainsNaN())
	{
		UE_LOG(LogManager, Error,
			TEXT("[DS] PositionCorrectionRejected Reason=InvalidTransform Player=%s Location=%s Rotation=%s Context=%s"),
			*GetNameSafe(PlayerState),
			*TargetLocation.ToString(),
			*TargetRotation.ToString(),
			Context ? Context : TEXT("<NULL>"));
		return;
	}

	ClearPendingServerPositionCorrection();
	PendingServerPositionCorrectionLocation = TargetLocation;
	PendingServerPositionCorrectionRotation = TargetRotation.GetNormalized();
	PendingServerPositionCorrectionContext = Context ? Context : TEXT("<NULL>");
	PendingServerPositionCorrectionSendCount = 0;
	bPendingServerPositionCorrection = true;

	SendPendingServerPositionCorrection(TEXT("Initial"));
	if (!bPendingServerPositionCorrection)
	{
		return;
	}

	if (!bGameplayInputLocked)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ServerPositionCorrectionRetryTimerHandle,
			this,
			&AMainPlayerController::RetryPendingServerPositionCorrection,
			ServerPositionCorrectionRetryIntervalSeconds,
			true);
	}
}

void AMainPlayerController::SendPendingServerPositionCorrection(const TCHAR* Reason)
{
	if (!bPendingServerPositionCorrection || !HasAuthority())
	{
		ClearPendingServerPositionCorrection();
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionStopped Reason=MissingPawn Player=%s Sends=%d Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	const FVector CurrentLocation = ControlledPawn->GetActorLocation();
	const FRotator CurrentRotation = ControlledPawn->GetActorRotation().GetNormalized();
	if (CurrentLocation.ContainsNaN() || CurrentRotation.ContainsNaN())
	{
		UE_LOG(LogManager, Error,
			TEXT("[DS] PositionCorrectionStopped Reason=InvalidPawnTransform Player=%s Location=%s Rotation=%s Context=%s"),
			*GetNameSafe(PlayerState),
			*CurrentLocation.ToString(),
			*CurrentRotation.ToString(),
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	const float LocationDrift = FVector::Dist(
		CurrentLocation,
		PendingServerPositionCorrectionLocation);
	const float RotationDrift = GetControllerRotationErrorDegrees(
		CurrentRotation,
		PendingServerPositionCorrectionRotation);
	if (PendingServerPositionCorrectionSendCount > 0 &&
		(LocationDrift > ServerPositionCorrectionDriftWarningDistance ||
			RotationDrift > ServerPositionCorrectionDriftWarningDegrees))
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionServerDrift Player=%s LocationDrift=%.2f RotationDrift=%.2f Sends=%d Context=%s"),
			*GetNameSafe(PlayerState),
			LocationDrift,
			RotationDrift,
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
	}

	// A correction always uses the latest transform still owned by the server.
	PendingServerPositionCorrectionLocation = CurrentLocation;
	PendingServerPositionCorrectionRotation = CurrentRotation;
	ClientSetLocation(CurrentLocation, CurrentRotation);
	ControlledPawn->ForceNetUpdate();
	++PendingServerPositionCorrectionSendCount;

	DS_LOG(TEXT("[DS] PositionCorrectionSend Player=%s Send=%d/%d Reason=%s Location=%s Rotation=%s Context=%s"),
		*GetNameSafe(PlayerState),
		PendingServerPositionCorrectionSendCount,
		ServerPositionCorrectionMaxSends,
		Reason ? Reason : TEXT("<NULL>"),
		*CurrentLocation.ToString(),
		*CurrentRotation.ToString(),
		*PendingServerPositionCorrectionContext);

	if (PendingServerPositionCorrectionSendCount >= ServerPositionCorrectionMaxSends)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=RetryLimit Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
	}
}

void AMainPlayerController::RetryPendingServerPositionCorrection()
{
	if (!bPendingServerPositionCorrection)
	{
		ClearPendingServerPositionCorrection();
		return;
	}

	if (!bGameplayInputLocked)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	SendPendingServerPositionCorrection(TEXT("LockedRetry"));
}

void AMainPlayerController::ClearPendingServerPositionCorrection()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerPositionCorrectionRetryTimerHandle);
	}

	bPendingServerPositionCorrection = false;
	PendingServerPositionCorrectionSendCount = 0;
	PendingServerPositionCorrectionContext.Reset();
}

void AMainPlayerController::Client_SetGameplayInputLocked_Implementation(bool bLocked, const FString& Context)
{
	ApplyGameplayInputLock(bLocked, *Context);
}

void AMainPlayerController::ApplyGameplayInputLock(bool bLocked, const TCHAR* Context)
{
	bGameplayInputLocked = bLocked;

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();

	if (bLocked)
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[%s] GameplayInputLock Locked=%d Context=%s Local=%d Phase=%d"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		bLocked ? 1 : 0,
		Context ? Context : TEXT("<NULL>"),
		IsLocalPlayerController() ? 1 : 0,
		static_cast<int32>(CurrentPhase));
}

bool AMainPlayerController::ForceCloseShopMode(const TCHAR* Context)
{
	if (!HasAuthority())
	{
		return false;
	}

	const bool bWasShopOpen = ApplyForceCloseShopMode(Context);
	Client_ForceCloseShopMode(FString(Context ? Context : TEXT("<NULL>")));
	return bWasShopOpen;
}

void AMainPlayerController::Client_ForceCloseShopMode_Implementation(const FString& Context)
{
	ApplyForceCloseShopMode(*Context);
}

bool AMainPlayerController::ApplyForceCloseShopMode(const TCHAR* Context)
{
	const bool bWasShopOpen = CurrentPhase == EGamePhase::Shop;
	const int32 PendingOptionCount = CurrentUpgradeOptions.Num();
	CurrentUpgradeOptions.Empty();

	if (!bWasShopOpen)
	{
		return false;
	}

	PhaseStack.Empty();
	ApplySwitchMode(EGamePhase::TPS);

	UE_LOG(LogTemp, Display, TEXT("[%s] Shop force closed Player=%s PendingOptions=%d Context=%s"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		*GetNameSafe(PlayerState),
		PendingOptionCount,
		Context ? Context : TEXT("<NULL>"));
	return true;
}

bool AMainPlayerController::Server_SwitchToLevel_Validate(FName LevelToUnload, FName LevelToLoad)
{
	return false;
}

void AMainPlayerController::Server_SwitchToLevel_Implementation(FName LevelToUnload, FName LevelToLoad)
{
}

void AMainPlayerController::Client_SwitchToLevel_Implementation(FName LevelToUnload, FName LevelToLoad)
{
	ClearPendingCardBundleExpectation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
	}

	PendingClientStreamLevelToUnload = LevelToUnload;
	PendingClientStreamLevelToLoad = LevelToLoad;
	PendingClientLevelReadinessRetryCount = 0;
	bPendingClientLevelReadyReported = false;

	if (!LevelToUnload.IsNone() && !IsControllerPersistentMainWorldTarget(LevelToUnload))
	{
		FLatentActionInfo UnloadInfo;
		UnloadInfo.CallbackTarget = this;
		UnloadInfo.ExecutionFunction = TEXT("OnClientStreamLevelUnloaded");
		UnloadInfo.Linkage = 1;
		UnloadInfo.UUID = ++ClientStreamingLatentActionId;
		UGameplayStatics::UnloadStreamLevel(GetWorld(), LevelToUnload, UnloadInfo, false);
	}

	if (IsControllerPersistentMainWorldTarget(LevelToLoad))
	{
		if (!TryReportPendingClientLevelReady(TEXT("PersistentWorldRequest")))
		{
			SchedulePendingClientLevelReadinessRetry();
		}
	}
	else if (!LevelToLoad.IsNone())
	{
		FLatentActionInfo LoadInfo;
		LoadInfo.CallbackTarget = this;
		LoadInfo.ExecutionFunction = TEXT("OnClientStreamLevelLoaded");
		LoadInfo.Linkage = 1;
		LoadInfo.UUID = ++ClientStreamingLatentActionId;
		UGameplayStatics::LoadStreamLevel(GetWorld(), LevelToLoad, true, false, LoadInfo);
		SchedulePendingClientLevelReadinessRetry();
	}
}

void AMainPlayerController::Client_SynchronizePhase_Implementation(EGamePhase ServerPhase)
{
	ApplySwitchMode(ServerPhase);
}

void AMainPlayerController::OnClientStreamLevelLoaded()
{
	if (!TryReportPendingClientLevelReady(TEXT("LoadCallback")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

void AMainPlayerController::OnClientStreamLevelUnloaded()
{
	UE_LOG(LogManagerCard, Verbose, TEXT("[CL] StreamLevelUnloaded Level=%s Phase=%d Local=%d"),
		*PendingClientStreamLevelToUnload.ToString(),
		static_cast<int32>(CurrentPhase),
		IsLocalPlayerController() ? 1 : 0);

	if (!TryReportPendingClientLevelReady(TEXT("UnloadCallback")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

bool AMainPlayerController::TryReportPendingClientLevelReady(const TCHAR* Context)
{
	if (bPendingClientLevelReadyReported || PendingClientStreamLevelToLoad.IsNone())
	{
		return bPendingClientLevelReadyReported;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bPersistentTarget = IsControllerPersistentMainWorldTarget(PendingClientStreamLevelToLoad);
	ULevelStreaming* TargetStreamingLevel = bPersistentTarget
		? nullptr
		: UGameplayStatics::GetStreamingLevel(World, PendingClientStreamLevelToLoad);

	bool bTargetFound = false;
	bool bTargetLoaded = false;
	bool bTargetVisible = false;
	FString TargetObjectName;

	if (bPersistentTarget)
	{
		const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
		bTargetFound = FName(*CurrentLevelName) == PendingClientStreamLevelToLoad;
		bTargetLoaded = bTargetFound && World->PersistentLevel != nullptr && World->HasBegunPlay();
		bTargetVisible = bTargetLoaded;
		TargetObjectName = CurrentLevelName;
	}
	else
	{
		bTargetFound = TargetStreamingLevel != nullptr;
		bTargetLoaded = TargetStreamingLevel && TargetStreamingLevel->IsLevelLoaded();
		bTargetVisible = TargetStreamingLevel && TargetStreamingLevel->IsLevelVisible();
		TargetObjectName = GetNameSafe(TargetStreamingLevel);
	}

	ULevelStreaming* StreamingLevelToUnload = PendingClientStreamLevelToUnload.IsNone()
		|| IsControllerPersistentMainWorldTarget(PendingClientStreamLevelToUnload)
		? nullptr
		: UGameplayStatics::GetStreamingLevel(World, PendingClientStreamLevelToUnload);
	const bool bUnloadComplete = !StreamingLevelToUnload
		|| (!StreamingLevelToUnload->IsLevelLoaded() && !StreamingLevelToUnload->IsLevelVisible());
	const bool bReady = bTargetFound && bTargetLoaded && bTargetVisible && bUnloadComplete;

	if (!bReady)
	{
		if (PendingClientLevelReadinessRetryCount == 0
			|| PendingClientLevelReadinessRetryCount % 40 == 0)
		{
			UE_LOG(LogManagerCard, Display,
				TEXT("[CL] LevelReadyWait Target=%s Mode=%s TargetObject=%s Found=%d Loaded=%d Visible=%d Unload=%s UnloadComplete=%d Retry=%d Context=%s Phase=%d Local=%d"),
				*PendingClientStreamLevelToLoad.ToString(),
				bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
				*TargetObjectName,
				bTargetFound ? 1 : 0,
				bTargetLoaded ? 1 : 0,
				bTargetVisible ? 1 : 0,
				*PendingClientStreamLevelToUnload.ToString(),
				bUnloadComplete ? 1 : 0,
				PendingClientLevelReadinessRetryCount,
				Context ? Context : TEXT("<NULL>"),
				static_cast<int32>(CurrentPhase),
				IsLocalPlayerController() ? 1 : 0);
		}

		return false;
	}

	World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
	bPendingClientLevelReadyReported = true;

	UE_LOG(LogManagerCard, Display,
		TEXT("[CL] LevelReadyAck Target=%s Mode=%s TargetObject=%s Unload=%s UnloadComplete=1 Retry=%d Context=%s Phase=%d Local=%d"),
		*PendingClientStreamLevelToLoad.ToString(),
		bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
		*TargetObjectName,
		*PendingClientStreamLevelToUnload.ToString(),
		PendingClientLevelReadinessRetryCount,
		Context ? Context : TEXT("<NULL>"),
		static_cast<int32>(CurrentPhase),
		IsLocalPlayerController() ? 1 : 0);

	Server_ReportStreamLevelLoaded(PendingClientStreamLevelToLoad, CurrentPhase);
	return true;
}

void AMainPlayerController::SchedulePendingClientLevelReadinessRetry()
{
	UWorld* World = GetWorld();
	if (!World || bPendingClientLevelReadyReported
		|| World->GetTimerManager().IsTimerActive(PendingClientLevelReadinessTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PendingClientLevelReadinessTimerHandle,
		this,
		&AMainPlayerController::RetryPendingClientLevelReadiness,
		0.25f,
		false);
}

void AMainPlayerController::RetryPendingClientLevelReadiness()
{
	if (UWorld* World = GetWorld())
	{
		// Executing one-shot timers still report active, so release the handle before rescheduling.
		World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
	}

	++PendingClientLevelReadinessRetryCount;
	if (!TryReportPendingClientLevelReady(TEXT("RetryTimer")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

bool AMainPlayerController::Server_ReportStreamLevelLoaded_Validate(FName LoadedLevel, EGamePhase ClientPhase)
{
	return !LoadedLevel.IsNone();
}

void AMainPlayerController::Server_ReportStreamLevelLoaded_Implementation(FName LoadedLevel, EGamePhase ClientPhase)
{
	if (!TryConsumeServerRpcRateLimit(
		StreamLevelAckRateLimitState,
		StreamLevelAckMinimumIntervalSeconds,
		TEXT("StreamLevelAck")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	GM->HandleClientStreamLevelLoaded(this, LoadedLevel, ClientPhase);
}

void AMainPlayerController::Client_ExpectCardBundle_Implementation(
	int32 Round,
	int32 BundleGeneration,
	const TArray<int32>& ExpectedInstanceIds)
{
	ClearPendingCardBundleExpectation();

	if (Round <= 0 || BundleGeneration <= 0 || ExpectedInstanceIds.IsEmpty())
	{
		UE_LOG(LogManagerCard, Error,
			TEXT("[CL] CardBundleExpectationRejected Round=%d Generation=%d Expected=%d"),
			Round,
			BundleGeneration,
			ExpectedInstanceIds.Num());
		return;
	}

	PendingClientCardBundleRound = Round;
	PendingClientCardBundleGeneration = BundleGeneration;
	PendingClientExpectedCardInstanceIds = ExpectedInstanceIds;
	PendingClientExpectedCardInstanceIds.Sort();
	PendingClientExpectedCardInstanceIds.SetNum(
		Algo::Unique(PendingClientExpectedCardInstanceIds));

	if (PendingClientExpectedCardInstanceIds.Num() != ExpectedInstanceIds.Num())
	{
		UE_LOG(LogManagerCard, Error,
			TEXT("[CL] CardBundleExpectationRejected Reason=DuplicateInstanceIds Round=%d Generation=%d Received=%d Unique=%d"),
			Round,
			BundleGeneration,
			ExpectedInstanceIds.Num(),
			PendingClientExpectedCardInstanceIds.Num());
		ClearPendingCardBundleExpectation();
		return;
	}

	if (!TryReportPendingCardBundleReady(TEXT("ExpectationReceived")))
	{
		SchedulePendingCardBundleReadinessRetry();
	}
}

bool AMainPlayerController::TryReportPendingCardBundleReady(const TCHAR* Context)
{
	if (bPendingClientCardBundleReadyReported
		|| PendingClientCardBundleGeneration <= 0
		|| PendingClientExpectedCardInstanceIds.IsEmpty())
	{
		return bPendingClientCardBundleReadyReported;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TSet<int32> VisibleInstanceIds;
	for (TActorIterator<ACardDropActor> It(World); It; ++It)
	{
		const ACardDropActor* CardActor = *It;
		if (!IsValid(CardActor)
			|| CardActor->GetCardInstanceId() <= 0
			|| CardActor->GetCardID() == ECardID::None
			|| CardActor->IsPickedUp()
			|| CardActor->IsHidden())
		{
			continue;
		}

		VisibleInstanceIds.Add(CardActor->GetCardInstanceId());
	}

	TArray<int32> MissingInstanceIds;
	for (const int32 ExpectedInstanceId : PendingClientExpectedCardInstanceIds)
	{
		if (!VisibleInstanceIds.Contains(ExpectedInstanceId))
		{
			MissingInstanceIds.Add(ExpectedInstanceId);
		}
	}

	const int32 ExpectedCount = PendingClientExpectedCardInstanceIds.Num();
	const int32 VisibleExpectedCount = ExpectedCount - MissingInstanceIds.Num();
	if (!MissingInstanceIds.IsEmpty())
	{
		if (PendingClientCardBundleRetryCount == 0
			|| PendingClientCardBundleRetryCount % 20 == 0)
		{
			FString MissingIdsText;
			for (int32 Index = 0; Index < MissingInstanceIds.Num(); ++Index)
			{
				if (Index > 0)
				{
					MissingIdsText += TEXT(",");
				}
				MissingIdsText += FString::FromInt(MissingInstanceIds[Index]);
			}

			UE_LOG(LogManagerCard, Display,
				TEXT("[CL] CardBundleReadyWait Round=%d Generation=%d Visible=%d/%d Missing=[%s] Retry=%d Context=%s Local=%d"),
				PendingClientCardBundleRound,
				PendingClientCardBundleGeneration,
				VisibleExpectedCount,
				ExpectedCount,
				*MissingIdsText,
				PendingClientCardBundleRetryCount,
				Context ? Context : TEXT("<NULL>"),
				IsLocalPlayerController() ? 1 : 0);
		}

		return false;
	}

	World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	bPendingClientCardBundleReadyReported = true;

	UE_LOG(LogManagerCard, Display,
		TEXT("[CL] CardBundleReadyAck Round=%d Generation=%d Visible=%d/%d Retry=%d Context=%s Local=%d"),
		PendingClientCardBundleRound,
		PendingClientCardBundleGeneration,
		VisibleExpectedCount,
		ExpectedCount,
		PendingClientCardBundleRetryCount,
		Context ? Context : TEXT("<NULL>"),
		IsLocalPlayerController() ? 1 : 0);

	Server_ReportCardBundleReady(
		PendingClientCardBundleRound,
		PendingClientCardBundleGeneration,
		VisibleExpectedCount);
	return true;
}

void AMainPlayerController::SchedulePendingCardBundleReadinessRetry()
{
	UWorld* World = GetWorld();
	if (!World
		|| bPendingClientCardBundleReadyReported
		|| PendingClientCardBundleGeneration <= 0
		|| World->GetTimerManager().IsTimerActive(PendingClientCardBundleReadinessTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PendingClientCardBundleReadinessTimerHandle,
		this,
		&AMainPlayerController::RetryPendingCardBundleReadiness,
		0.25f,
		false);
}

void AMainPlayerController::RetryPendingCardBundleReadiness()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	}

	++PendingClientCardBundleRetryCount;
	if (!TryReportPendingCardBundleReady(TEXT("RetryTimer")))
	{
		SchedulePendingCardBundleReadinessRetry();
	}
}

void AMainPlayerController::ClearPendingCardBundleExpectation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	}

	PendingClientCardBundleRound = 0;
	PendingClientCardBundleGeneration = 0;
	PendingClientCardBundleRetryCount = 0;
	bPendingClientCardBundleReadyReported = false;
	PendingClientExpectedCardInstanceIds.Reset();
}

bool AMainPlayerController::Server_ReportCardBundleReady_Validate(
	int32 Round,
	int32 BundleGeneration,
	int32 VisibleCount)
{
	return Round > 0 && BundleGeneration > 0 && VisibleCount > 0 && VisibleCount <= 64;
}

void AMainPlayerController::Server_ReportCardBundleReady_Implementation(
	int32 Round,
	int32 BundleGeneration,
	int32 VisibleCount)
{
	if (!TryConsumeServerRpcRateLimit(
		CardBundleAckRateLimitState,
		CardBundleAckMinimumIntervalSeconds,
		TEXT("CardBundleAck")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	GM->HandleClientCardBundleReady(this, Round, BundleGeneration, VisibleCount);
}

bool AMainPlayerController::Server_SwitchState_Validate(EGamePhase NewPhase)
{
	return false;
}

void AMainPlayerController::Server_SwitchState_Implementation(EGamePhase NewPhase)
{
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

bool AMainPlayerController::Server_PushMode_Validate(EGamePhase NewPhase)
{
	return NewPhase == EGamePhase::Shop;
}

void AMainPlayerController::Server_PushMode_Implementation(EGamePhase NewPhase)
{
	if (!TryConsumeServerRpcRateLimit(
		ShopPushRateLimitState,
		ShopModeMinimumIntervalSeconds,
		TEXT("ShopPush")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (NewPhase != EGamePhase::Shop ||
		CurrentPhase != EGamePhase::TPS ||
		!GM ||
		!GM->IsShopRequestAllowed() ||
		!PS ||
		PS->CurPlayerData.CurrentHP <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Shop push rejected Player=%s Requested=%d Current=%d"),
			*GetNameSafe(PlayerState),
			static_cast<int32>(NewPhase),
			static_cast<int32>(CurrentPhase));
		return;
	}

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
	if (!TryConsumeServerRpcRateLimit(
		ShopPopRateLimitState,
		ShopModeMinimumIntervalSeconds,
		TEXT("ShopPop")))
	{
		return;
	}

	if (CurrentPhase != EGamePhase::Shop)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] Shop pop rejected Player=%s Current=%d"),
			*GetNameSafe(PlayerState),
			static_cast<int32>(CurrentPhase));
		return;
	}

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
	if (!TryConsumeServerRpcRateLimit(
		ShopRandomRollRateLimitState,
		ShopRandomRollMinimumIntervalSeconds,
		TEXT("ShopRandomRoll")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!GM ||
		!PS ||
		!GM->IsShopRequestAllowed() ||
		CurrentPhase != EGamePhase::Shop ||
		PS->CurPlayerData.CurrentHP <= 0 ||
		PS->LastRandomUpgradeClaimedRound == GM->GetCurrentRound() ||
		CurrentUpgradeOptions.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DS] RandomUpgrade request rejected Player=%s Phase=%d Round=%d ClaimedRound=%d PendingOptions=%d"),
			*GetNameSafe(PlayerState),
			static_cast<int32>(CurrentPhase),
			GM ? GM->GetCurrentRound() : INDEX_NONE,
			PS ? PS->LastRandomUpgradeClaimedRound : INDEX_NONE,
			CurrentUpgradeOptions.Num());
		return;
	}

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
	DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Rand Status UP")));

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

bool AMainPlayerController::Server_SelectUpgradeOption_Validate(int32 SelectedIndex)
{
	return SelectedIndex >= 0 && SelectedIndex < 3;
}

void AMainPlayerController::Server_SelectUpgradeOption_Implementation(int32 SelectedIndex)
{
	if (!TryConsumeServerRpcRateLimit(
		ShopRandomSelectionRateLimitState,
		ShopUpgradeSelectionMinimumIntervalSeconds,
		TEXT("ShopRandomSelection")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!GM ||
		!PS ||
		!GM->IsShopRequestAllowed() ||
		CurrentPhase != EGamePhase::Shop ||
		PS->CurPlayerData.CurrentHP <= 0 ||
		PS->LastRandomUpgradeClaimedRound == GM->GetCurrentRound())
	{
		return;
	}

	if (!CurrentUpgradeOptions.IsValidIndex(SelectedIndex)) return;

	const FRandomCardOption& ChosenOption = CurrentUpgradeOptions[SelectedIndex];

	PS->ApplyCardUpgrade(ChosenOption.RolledStats);

	PS->LastRandomUpgradeClaimedRound = GM->GetCurrentRound();
	CurrentUpgradeOptions.Empty();
}

bool AMainPlayerController::Server_RequestDiscardCard_Validate(int32 CardInstanceId)
{
	return CardInstanceId > 0;
}

void AMainPlayerController::Server_RequestDiscardCard_Implementation(int32 CardInstanceId)
{
	if (!TryConsumeServerRpcRateLimit(
		CardDiscardRateLimitState,
		CardDiscardMinimumIntervalSeconds,
		TEXT("CardDiscard")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	const bool bAllowedInShop = GM &&
		GM->IsShopRequestAllowed() &&
		CurrentPhase == EGamePhase::Shop;
	const bool bAllowedInBattle = GM &&
		GM->IsBattleRoyalePhase() &&
		CurrentPhase == EGamePhase::TPS;
	if (!PS || PS->CurPlayerData.CurrentHP <= 0 ||
		(!bAllowedInShop && !bAllowedInBattle))
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS][Security] CardDiscardRejected Player=%s CardInstanceId=%d ControllerPhase=%d ShopAllowed=%d BattleAllowed=%d Alive=%d"),
			*GetNameSafe(PlayerState),
			CardInstanceId,
			static_cast<int32>(CurrentPhase),
			bAllowedInShop ? 1 : 0,
			bAllowedInBattle ? 1 : 0,
			PS && PS->CurPlayerData.CurrentHP > 0 ? 1 : 0);
		return;
	}

	UCardGameService* CardGameService = GM ? GM->GetCardGameService() : nullptr;
	if (!CardGameService ||
		!CardGameService->DiscardOwnedCard(
			this,
			CardInstanceId,
			bAllowedInBattle,
			bAllowedInShop ? TEXT("Shop") : TEXT("BattleRoyale")))
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS][Security] CardDiscardRejected Player=%s CardInstanceId=%d Reason=ServiceRejected"),
			*GetNameSafe(PlayerState),
			CardInstanceId);
	}
}

void AMainPlayerController::Client_NotifyHitConfirmed_Implementation()
{
	TObjectPtr<UUIHandler>* Handler = UIHandlerMap.Find(EGamePhase::TPS);
	if (!Handler || !*Handler) return;

	if (UTpsPlayerMainHUD* HUD = Cast<UTpsPlayerMainHUD>((*Handler)->GetWidget()))
	{
		HUD->ShowHitMarker();
	}
}

EUpgradeType AMainPlayerController::GetStaticUpgradeTypeFromIndex(int32 Index)
{
	// UI에 고정 능력치 상승 버튼들이 배치된 순서대로 대응시킵니다.
	switch (Index)
	{
	case 0:
		DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_Health")));

		return EUpgradeType::Player_Health;
	case 1:
		DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_MoveSpeed")));

		return EUpgradeType::Player_MoveSpeed;
	case 2:
		DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("Player_HealthRegeneration")));

		return EUpgradeType::Player_HealthRegeneration;
	default:
		DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("None")));

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
bool AMainPlayerController::Server_SelectStaticUpgradeOption_Validate(int32 SelectedIndex)
{
	return SelectedIndex >= 0 && SelectedIndex < 3;
}

void AMainPlayerController::Server_SelectStaticUpgradeOption_Implementation(int32 SelectedIndex) {
	if (!TryConsumeServerRpcRateLimit(
		ShopStaticUpgradeRateLimitState,
		ShopUpgradeSelectionMinimumIntervalSeconds,
		TEXT("ShopStaticUpgrade")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (CurrentPhase != EGamePhase::Shop ||
		!GM ||
		!GM->IsShopRequestAllowed() ||
		!PS ||
		PS->CurPlayerData.CurrentHP <= 0)
	{
		return;
	}

	EUpgradeType UpgradeType = GetStaticUpgradeTypeFromIndex(SelectedIndex);
	if (UpgradeType == EUpgradeType::None)
		return;

	int32 CurrentLevel = GetCurrentUpgradeLevel(PS, UpgradeType);
	constexpr int32 MaxUpgradeLevel = 5;
	if (CurrentLevel >= MaxUpgradeLevel)
	{
		return;
	}

	int32 Cost = GetStaticUpgradeCost(UpgradeType, CurrentLevel);
	if (PS->CurPlayerData.HoldingGold < Cost)
	{
		return;
	}

	PS->AddGold(-Cost);
	PS->Server_ApplyUpgrad_Implementation(UpgradeType);
	DS_SCREEN(-1, 8.f, FColor::Cyan, FString::Printf(TEXT("static Status UP")));

}

bool AMainPlayerController::Server_SetUITimer_Validate(int32 time)
{
	return false;
}

void AMainPlayerController::Server_SetUITimer_Implementation(int32 time)
{
	UE_LOG(LogTemp, Warning, TEXT("[DS] Rejected client UI timer request Player=%s Value=%d"),
		*GetNameSafe(PlayerState),
		time);
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
	if (!TryConsumeCardPickupRequest())
	{
		return;
	}

    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM || !GM->GetCardGameService())
    {
        return;
    }

    GM->GetCardGameService()->TryPickupCard(this, TargetCard);
}




bool AMainPlayerController::Server_RevealSeotdaCard_Validate(bool bCard0, bool bCard1, bool bCard2)
{
    return true;
}

void AMainPlayerController::Server_RevealSeotdaCard_Implementation(bool bCard0, bool bCard1, bool bCard2)
{
	if (!TryConsumeServerRpcRateLimit(
		SeotdaRevealRateLimitState,
		SeotdaRevealMinimumIntervalSeconds,
		TEXT("SeotdaReveal")))
	{
		Client_ReceiveSeotdaRevealResult(false, TEXT("RateLimited"));
		return;
	}

    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM || !GM->GetCardGameService())
    {
		Client_ReceiveSeotdaRevealResult(false, TEXT("ServerUnavailable"));
        return;
    }

	FString FailureReason;
	const bool bAccepted = GM->GetCardGameService()->RevealSeotdaCard(
		this,
		bCard0,
		bCard1,
		bCard2,
		FailureReason);

	Client_ReceiveSeotdaRevealResult(
		bAccepted,
		bAccepted ? FString(TEXT("Accepted")) : FailureReason);
}

void AMainPlayerController::Client_ReceiveSeotdaRevealResult_Implementation(
	bool bAccepted,
	const FString& Reason)
{
	++SeotdaUiRevealResultSerial;
	bSeotdaUiRevealAccepted = bAccepted;
	SeotdaUiRevealResultReason = Reason;

	if (bAccepted)
	{
		bSeotdaUiMyRevealConfirmed = true;
	}

	UE_LOG(LogTemp, Display,
		TEXT("[CL] SeotdaRevealResult Serial=%d Accepted=%d Reason=%s"),
		SeotdaUiRevealResultSerial,
		bAccepted ? 1 : 0,
		*Reason);
}

bool AMainPlayerController::Server_SubmitSeotdaSelection_Validate(bool bCard0, bool bCard1, bool bCard2)
{
    return true;
}

void AMainPlayerController::Server_SubmitSeotdaSelection_Implementation(bool bCard0, bool bCard1, bool bCard2)
{
	if (!TryConsumeServerRpcRateLimit(
		SeotdaSelectionRateLimitState,
		SeotdaSelectionMinimumIntervalSeconds,
		TEXT("SeotdaSelection")))
	{
		Client_ReceiveSeotdaSelectionResult(false, TEXT("RateLimited"));
		return;
	}

    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM || !GM->GetCardGameService())
    {
		Client_ReceiveSeotdaSelectionResult(false, TEXT("ServerUnavailable"));
        return;
    }

	FString FailureReason;
	const bool bAccepted = GM->GetCardGameService()->SubmitSeotdaSelection(
		this,
		bCard0,
		bCard1,
		bCard2,
		FailureReason);

	Client_ReceiveSeotdaSelectionResult(
		bAccepted,
		bAccepted ? FString(TEXT("Accepted")) : FailureReason);
}

void AMainPlayerController::Client_ReceiveSeotdaSelectionResult_Implementation(
	bool bAccepted,
	const FString& Reason)
{
	++SeotdaUiSelectionResultSerial;
	bSeotdaUiSelectionAccepted = bAccepted;
	SeotdaUiSelectionResultReason = Reason;

	if (bAccepted)
	{
		bSeotdaUiMySubmitted = true;
	}

	UE_LOG(LogTemp, Display,
		TEXT("[CL] SeotdaSelectionResult Serial=%d Accepted=%d Reason=%s"),
		SeotdaUiSelectionResultSerial,
		bAccepted ? 1 : 0,
		*Reason);
}


bool AMainPlayerController::Server_RequestSeotdaBetAction_Validate(EBettingAction Action)
{
	return Action > EBettingAction::None && Action <= EBettingAction::AllIn;
}

void AMainPlayerController::Server_RequestSeotdaBetAction_Implementation(EBettingAction Action)
{
	if (!TryConsumeServerRpcRateLimit(
		SeotdaBetRateLimitState,
		SeotdaBetMinimumIntervalSeconds,
		TEXT("SeotdaBet")))
	{
		return;
	}

    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    if (!GM || !GM->GetCardGameService())
    {
        return;
    }

    GM->GetCardGameService()->SubmitSeotdaBetAction(this, Action);
}
void AMainPlayerController::Client_ShowSeotdaResult_Implementation(const FString& ResultText)
{
UE_LOG(LogTemp, Warning, TEXT("[CL] Seotda Result: %s"), *ResultText);

    SeotdaUiLastResultText = ResultText;
    bSeotdaUiMatchEnded = ResultText.Contains(TEXT("[MATCH END]"));

    if (bSeotdaUiMatchEnded)
    {
        if (UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>())
        {
            GI->MarkReturnToRoomAfterMatch();
        }
    }

if (GEngine)
{
DS_SCREEN(
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
bool bMyRevealConfirmed,
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
bSeotdaUiMyRevealConfirmed = bMyRevealConfirmed;
bSeotdaUiMySubmitted = bMySubmitted;
bSeotdaUiMyFolded = bMyFolded;
bSeotdaUiRoundResolved = bRoundResolved;

UE_LOG(LogTemp, Warning,
TEXT("[CL] SeotdaState Round=%d Betting=%d Turn=%s Pot=%d CurrentBet=%d MyBet=%d NeedCall=%d MyTurn=%d Revealed=%d Submitted=%d Folded=%d Resolved=%d"),
SeotdaUiRound,
bSeotdaUiBettingActive ? 1 : 0,
*SeotdaUiCurrentTurnPlayerName,
SeotdaUiPot,
SeotdaUiCurrentBet,
SeotdaUiMyBetMoney,
SeotdaUiNeedCall,
bSeotdaUiMyTurn ? 1 : 0,
bSeotdaUiMyRevealConfirmed ? 1 : 0,
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

    UE_LOG(LogTemp, Warning, TEXT("[CL] ReturnToLobbyFromMatchEnd OpenLevel /Game/Lobby/System/Lobby_Stage"));

    bSeotdaUiMatchEnded = false;
    SeotdaUiLastResultText.Empty();

    if (UUManagerGameInstance* GI = GetGameInstance<UUManagerGameInstance>())
    {
        GI->MarkReturnToRoomAfterMatch();
    }

    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }

    UWidgetLayoutLibrary::RemoveAllWidgets(this);

    bShowMouseCursor = true;
    SetInputMode(FInputModeGameAndUI());

    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Lobby/System/Lobby_Stage")), true);
}

