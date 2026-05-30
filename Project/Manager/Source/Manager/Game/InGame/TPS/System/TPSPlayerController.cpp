// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Manager.h"
#include "Default/Ability/CustomASC.h"
#include "Net/UnrealNetwork.h"
#include "Game/InGame/TPS/System/TPSCharacter.h"
#include "Game/InGame/TPS/System/TPSGameMode.h"
#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"

#define VALIDATE_CHARACTER\
	ATPSCharacter* MyChar = Cast<ATPSCharacter>(GetPawn());\
	if (!MyChar) return;

void ATPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	TPS_UI();
}

void ATPSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->RemoveFromParent();
		PlayerHUDWidget = nullptr;
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	Super::EndPlay(EndPlayReason);
}

void ATPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : TPSMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_Move) EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ATPSPlayerController::Input_Move);
		if (IA_Look) EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ATPSPlayerController::Input_Look);
		if (IA_Jump) EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &ATPSPlayerController::Input_Jump);
		if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Started, this, &ATPSPlayerController::Input_StartFire);
		if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Completed, this, &ATPSPlayerController::Input_StopFire);
		if (IA_Aim)
		{
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &ATPSPlayerController::Input_Aim);
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Completed, this, &ATPSPlayerController::Input_AimEnd);
		}
	}
}

void ATPSPlayerController::Input_Move(const FInputActionValue& Value)
{
	VALIDATE_CHARACTER
	MyChar->Move(Value);
}

void ATPSPlayerController::Input_Look(const FInputActionValue& Value)
{
	VALIDATE_CHARACTER

	MyChar->Look(Value);
}

void ATPSPlayerController::Input_Jump()
{
	VALIDATE_CHARACTER
	if (UCustomASC* ASC = MyChar->GetCustomASC())
	{
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Jump")));
	}
}

void ATPSPlayerController::Input_StartFire()
{
	VALIDATE_CHARACTER
	MyChar->GetEquippedGun()->Setting->StartLoopFire();
}

void ATPSPlayerController::Input_StopFire()
{
	VALIDATE_CHARACTER
	MyChar->GetEquippedGun()->Setting->StopLoopFire();
}

void ATPSPlayerController::Input_Aim()
{
	VALIDATE_CHARACTER
	if (UCustomASC* ASC = MyChar->GetCustomASC())
	{
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
	}
}

void ATPSPlayerController::Input_AimEnd()
{
	VALIDATE_CHARACTER
	if (UCustomASC* ASC = MyChar->GetCustomASC())
	{
		ASC->CancelAbilitiesWithTag(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim"))));
	}
}

void ATPSPlayerController::TPS_UI() {
	if (IsLocalPlayerController())
	{
		if (PlayerTPSUI)
		{
			PlayerHUDWidget = CreateWidget<UTpsPlayerMainHUD>(this, PlayerTPSUI);
			if (PlayerHUDWidget)
			{
				PlayerHUDWidget->AddToViewport();
				if (ATPSCharacter* MyChar = Cast<ATPSCharacter>(GetPawn()))
					PlayerHUDWidget->BindCharacterState(MyChar->CharacterState);
			}
		}
	}
}

bool ATPSPlayerController::Server_SendAction_Validate(FName ActionName)
{
	return true;
}

void ATPSPlayerController::Server_SendAction_Implementation(FName ActionName)
{
	if (ATPSGameMode* GM = Cast<ATPSGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnPlayerAction(this, ActionName);
	}
}