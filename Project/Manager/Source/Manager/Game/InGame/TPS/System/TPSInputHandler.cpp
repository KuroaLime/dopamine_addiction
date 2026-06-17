// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/TPSInputHandler.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Game/InGame/MainPlayerState.h"

UTPSInputHandler::UTPSInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTPSInputHandler::BeginPlay()
{
	Super::BeginPlay();
}

void UTPSInputHandler::InputActivate()
{
	Super::InputActivate();

	if (!OwnerController) return;
	OwnerController->bShowMouseCursor = false;
	OwnerController->SetInputMode(FInputModeGameOnly());
}

void UTPSInputHandler::InputDeactivate()
{
	Super::InputDeactivate();
}

void UTPSInputHandler::AddInputMappingContexts()
{
	for (UInputMappingContext* Context : MappingContexts)
	{
		AddMappingContext(Context, 0);
	}

	if (MappingContext)
	{
		AddMappingContext(MappingContext, 0);
	}
}

void UTPSInputHandler::RemoveInputMappingContexts()
{
	for (UInputMappingContext* Context : MappingContexts)
	{
		RemoveMappingContext(Context);
	}

	if (MappingContext)
	{
		RemoveMappingContext(MappingContext);
	}
}

void UTPSInputHandler::SetupInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if(!EnhancedInputComponent) return;

	if (IA_Move) EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &UTPSInputHandler::Input_Move);
	if (IA_Look) EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &UTPSInputHandler::Input_Look);
	if (IA_Jump) EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &UTPSInputHandler::Input_Jump);
	if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Started, this, &UTPSInputHandler::Input_StartFire);
	if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Completed, this, &UTPSInputHandler::Input_StopFire);
	if (IA_Aim)
	{
		EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Started, this, &UTPSInputHandler::Input_Aim);
		EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Completed, this, &UTPSInputHandler::Input_AimEnd);
	}
	if (IA_Shop)
	{
		EnhancedInputComponent->BindAction(IA_Shop, ETriggerEvent::Completed, this, &UTPSInputHandler::Input_Shop);
	}
}

void UTPSInputHandler::Input_Move(const FInputActionValue& Value)
{
	if (!OwnerController) return;

	APawn* OwnerPawn = OwnerController->GetPawn();
	if (!OwnerPawn) return;

	const FVector2D MoveVector = Value.Get<FVector2D>();
	const FRotator Rotation = OwnerController->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	OwnerPawn->AddMovementInput(ForwardDir, MoveVector.Y);
	OwnerPawn->AddMovementInput(RightDir, MoveVector.X);
}

void UTPSInputHandler::Input_Look(const FInputActionValue& Value)
{
	if (!OwnerController) return;

	const FVector2D LookVector = Value.Get<FVector2D>();
	OwnerController->AddYawInput(LookVector.X);
	OwnerController->AddPitchInput(LookVector.Y);
}

void UTPSInputHandler::Input_Jump()
{
	if (UPFGASC* ASC = ResolveOwnerASC())
	{
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Jump")));
	}
}

void UTPSInputHandler::Input_StartFire()
{
	if (!OwnerController) return;

	IAbilityOwnerInterface* OwnerInterface = Cast<IAbilityOwnerInterface>(OwnerController->GetPawn());
	if (!OwnerInterface) return;

	AWeapon* EquippedGun = Cast<AWeapon>(OwnerInterface->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting) return;

	EquippedGun->Setting->StartLoopFire();



}

void UTPSInputHandler::Input_StopFire()
{
	if (!OwnerController) return;

	IAbilityOwnerInterface* OwnerInterface = Cast<IAbilityOwnerInterface>(OwnerController->GetPawn());
	if (!OwnerInterface) return;

	AWeapon* EquippedGun = Cast<AWeapon>(OwnerInterface->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting) return;

	EquippedGun->Setting->StopLoopFire();
}

void UTPSInputHandler::Input_Aim()
{
	if (UPFGASC* ASC = ResolveOwnerASC())
	{
		ASC->TryActivateAbilityByTag(
			FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
	}
}

void UTPSInputHandler::Input_AimEnd()
{
	if (UPFGASC* ASC = ResolveOwnerASC())
	{
		ASC->CancelAbilitiesWithTag(
			FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim"))));
	}
}

void UTPSInputHandler::Input_Shop()
{
	if (UPFGASC* ASC = ResolveOwnerASC())
	{		
		ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Input.Shop")));
	}
}