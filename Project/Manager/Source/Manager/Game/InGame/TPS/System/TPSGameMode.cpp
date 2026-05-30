// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSGameMode.h"
#include "TimerManager.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "PlayerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Game/InGame/TPS/System/TPSCharacter.h"
#include "Game/InGame/TPS/System/TPSPlayerController.h"
#include "Game/InGame/TPS/System/TPSGameState.h"

#define VALIDATE_GS if (!GetGS()) return;
#define VALIDATE_GS_RET(ret) if (!GetGS()) return ret;
#define VALIDATE_SPAWNMANAGER if (!SpawnManager) return;

ATPSGameMode::ATPSGameMode()
{
	SpawnManager = CreateDefaultSubobject<USpawnManagerComponent>(TEXT("SpawnManager"));
}

void ATPSGameMode::BeginPlay()
{
	Super::BeginPlay();

	pTGS = GetGameState<ATPSGameState>();

	VALIDATE_GS
	pTGS->RemainingTime = 10;

	bGameStarted = true; // Ãß°¡

	if (!GetWorldTimerManager().IsTimerActive(RoundTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ATPSGameMode::RoundTimerTick, 1.0f, true);
	}

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->OnPlayerActionEvent.AddUObject(this, &ATPSGameMode::OnPlayerAction);
	}

	VALIDATE_SPAWNMANAGER
}

void ATPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void ATPSGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

FORCEINLINE ATPSGameState* ATPSGameMode::GetGS() const
{
	if (pTGS)
	{
		return pTGS;
	}

	ATPSGameMode* MutableThis = const_cast<ATPSGameMode*>(this);
	MutableThis->pTGS = Cast<ATPSGameState>(GetWorld()->GetGameState());

	return pTGS;
}

void ATPSGameMode::OnPlayerAction(AActor* Executor, FName ActionName)
{
	
}

void ATPSGameMode::StartBattleRoyalePhase()
{
	VALIDATE_GS
	pTGS->RemainingTime = BattleRoyaleDuration;

	if (!GetWorldTimerManager().IsTimerActive(RoundTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ATPSGameMode::RoundTimerTick, 1.0f, true);
	}
}

void ATPSGameMode::RoundTimerTick()
{
	if (!bGameStarted)
	{
		return;
	}

	VALIDATE_GS
	pTGS->RemainingTime--;

	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 1.1f, FColor::Yellow,
			FString::Printf(TEXT("Remaining Time: %d"), pTGS->RemainingTime));
	}


	if (pTGS->OnTimeUpdated.IsBound())
	{
		pTGS->OnTimeUpdated.Broadcast(pTGS->RemainingTime);
	}

	if (pTGS->RemainingTime <= 0)
	{
		GetWorldTimerManager().ClearTimer(RoundTimerHandle);

		//StartBattleRoyalePhase();

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ATPSPlayerController* PC = Cast<ATPSPlayerController>(It->Get()))
			{
				PC->Client_SwitchToLevel(TEXT("TPS_Game_Stage"), TEXT("Card_Game_Stage"));

				PC->SetInputMode(FInputModeGameAndUI());
				PC->bShowMouseCursor = true;
			}
		}
	}
}

AActor* ATPSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (SpawnManager)
	{
		if (SpawnManager->GetAvailableSpawnCount() == 0)
		{
			SpawnManager->InitializeSpawnPoints();
		}

		AA_Spawn* RandomSpawn = SpawnManager->GetUniqueRandomSpawnActor();

		if (RandomSpawn)
		{
			return RandomSpawn;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* ATPSGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform OffsetTransform = SpawnTransform;
	FVector NewLocation = OffsetTransform.GetLocation() + FVector(0.0f, 0.0f, 100.0f);

	OffsetTransform.SetLocation(NewLocation);

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OffsetTransform);
}