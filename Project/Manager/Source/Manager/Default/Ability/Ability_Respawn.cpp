// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/Ability/Ability_Respawn.h"
#include "Game/InGame/MainCharacter.h"
#include "Game/InGame/MainGameMode.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

UAbility_Respawn::UAbility_Respawn()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Respawn")));
}

void UAbility_Respawn::LocalActivateWithOwner(AActor* InOwner)
{
    // The server switches the client to TPS only after placement and state restoration succeed.
}

void UAbility_Respawn::LocalCancelWithOwner(AActor* InOwner)
{
}

void UAbility_Respawn::ActivateAbility()
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
    {
        EndAbility(true);
        return;
    }

    ServerRespawnRetryCount = 0;
    TryCompleteServerRespawn();
}

void UAbility_Respawn::TryCompleteServerRespawn()
{
    if (!IsActive())
    {
        return;
    }

    UWorld* World = GetWorld();
    AMainCharacter* MainCharacter = Cast<AMainCharacter>(OwnerCharacter);
    AMainGameMode* GameMode = World ? Cast<AMainGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!World || !MainCharacter)
    {
        EndAbility(true);
        return;
    }

    if (GameMode && GameMode->TryRespawnPlayerAuthoritatively(MainCharacter, TEXT("DeathRespawn")))
    {
        EndAbilityNow();
        return;
    }

    ++ServerRespawnRetryCount;
    if (ServerRespawnRetryCount == 1 || ServerRespawnRetryCount % 10 == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DS] RespawnPending Player=%s Attempt=%d Reason=SpawnOrPlacementNotReady"),
            *GetNameSafe(MainCharacter->GetPlayerState()),
            ServerRespawnRetryCount);
    }

    World->GetTimerManager().SetTimer(
        ServerRespawnRetryTimerHandle,
        this,
        &UAbility_Respawn::TryCompleteServerRespawn,
        1.f,
        false);
}

void UAbility_Respawn::EndAbility(bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ServerRespawnRetryTimerHandle);
    }
    ServerRespawnRetryCount = 0;
    Super::EndAbility(bWasCancelled);
}
