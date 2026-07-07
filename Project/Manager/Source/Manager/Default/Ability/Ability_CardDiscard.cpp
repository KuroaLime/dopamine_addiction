// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/Ability/Ability_CardDiscard.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Data/CameraStateComponent.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "GameFramework/Character.h"

UAbility_CardDiscard::UAbility_CardDiscard()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.CardDiscard")));
    bLocalOnly = true;
}

void UAbility_CardDiscard::LocalActivateWithOwner(AActor* InOwner)
{

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("LocalActivateWithOwner"));
    }
    StartHold(InOwner, InOwner->GetWorld());
}

void UAbility_CardDiscard::ActivateAbility()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("ActivateAbility"));
    }
    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        StartHold(OwnerCharacter, GetWorld());
    }
}

void UAbility_CardDiscard::LocalCancelWithOwner(AActor* InOwner)
{
    CancelHold(InOwner, InOwner->GetWorld());
}

void UAbility_CardDiscard::EndAbility(bool bWasCancelled)
{
    if (bWasCancelled)
    {
        CancelHold(OwnerCharacter, GetWorld());
    }
    Super::EndAbility(bWasCancelled);
}

void UAbility_CardDiscard::StartHold(AActor* InOwner, UWorld* World)
{
    if (!World) return;

    bHoldConfirmed = false;
    HoldOwner = InOwner;

    World->GetTimerManager().SetTimer(
        HoldConfirmTimerHandle, this, &UAbility_CardDiscard::ConfirmHold,
        HoldThreshold, false);

}

void UAbility_CardDiscard::ConfirmHold()
{
    bHoldConfirmed = true;
    if (AActor* Owner = HoldOwner.Get())
    {
        if (UTpsPlayerMainHUD* HUD = ResolveHUD(Owner))
        {
            HUD->ConfirmDiscardSelectedCard();
        }
    }
    EndAbilityNow();
}

void UAbility_CardDiscard::CancelHold(AActor* InOwner, UWorld* World)
{
    if (World)
    {
        World->GetTimerManager().ClearTimer(HoldConfirmTimerHandle);
    }

    if (!bHoldConfirmed)
    {
        if (UTpsPlayerMainHUD* HUD = ResolveHUD(InOwner))
        {
            HUD->CycleDiscardSelection();
        }
    }
}

UTpsPlayerMainHUD* UAbility_CardDiscard::ResolveHUD(AActor* InOwner) const
{
    ACharacter* Character = Cast<ACharacter>(InOwner);
    if (!Character) return nullptr;

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC) return nullptr;

    UUIHandler* UIHandler = PC->FindComponentByClass<UUIHandler>();
    if (!UIHandler) return nullptr;

    return Cast<UTpsPlayerMainHUD>(UIHandler->GetWidget());
}