// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game/InGame/ManagerPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Manager.h"
#include "Default/Ability/CustomASC.h"
#include "Game/InGame/ManagerCharacter.h" 
#include "Net/UnrealNetwork.h"
#include "ManagerGameMode.h"
#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Game/InGame/Card/UI/CardPlayerMainHUD.h" 
#include "Widgets/Input/SVirtualJoystick.h"


void AManagerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogManager, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
	TPS_UI();
}
void AManagerPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void AManagerPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_Move) EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AManagerPlayerController::Input_Move);
		if (IA_Look) EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AManagerPlayerController::Input_Look);
		if (IA_Jump) EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Jump);
		if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Started, this, &AManagerPlayerController::Input_StartFire);
		if (IA_Fire) EnhancedInputComponent->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AManagerPlayerController::Input_StopFire);
		if (IA_Aim)
		{
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &AManagerPlayerController::Input_Aim);
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Completed, this, &AManagerPlayerController::Input_AimEnd);
		}
		if (IA_PickUp) EnhancedInputComponent->BindAction(IA_PickUp, ETriggerEvent::Started, this, &AManagerPlayerController::Input_PickUp);
		if (IA_Drop) EnhancedInputComponent->BindAction(IA_Drop, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Drop);
		if (IA_Throw) EnhancedInputComponent->BindAction(IA_Throw, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Throw);
		if (IA_Skill00) EnhancedInputComponent->BindAction(IA_Skill00, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Skill00);

		if (IA_Check) EnhancedInputComponent->BindAction(IA_Check, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Check);
		if (IA_Call)  EnhancedInputComponent->BindAction(IA_Call, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Call);
		if (IA_Half)  EnhancedInputComponent->BindAction(IA_Half, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Half);
		if (IA_Die)   EnhancedInputComponent->BindAction(IA_Die, ETriggerEvent::Started, this, &AManagerPlayerController::Input_Die);
		if (IA_AllIn) EnhancedInputComponent->BindAction(IA_AllIn, ETriggerEvent::Started, this, &AManagerPlayerController::Input_AllIn);

		if (IA_SelectCard1)
			EnhancedInputComponent->BindAction(IA_SelectCard1, ETriggerEvent::Started, this, &AManagerPlayerController::Input_SelectCard1);

		if (IA_SelectCard2)
			EnhancedInputComponent->BindAction(IA_SelectCard2, ETriggerEvent::Started, this, &AManagerPlayerController::Input_SelectCard2);

		if (IA_SelectCard3)
			EnhancedInputComponent->BindAction(IA_SelectCard3, ETriggerEvent::Started, this, &AManagerPlayerController::Input_SelectCard3);

		if (IA_ConfirmSelection)
			EnhancedInputComponent->BindAction(IA_ConfirmSelection, ETriggerEvent::Started, this, &AManagerPlayerController::Input_ConfirmSelection);

		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AManagerPlayerController::ToggleModeTest);
	}
}

void AManagerPlayerController::SetSeotdaMode(bool bEnable)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (bEnable)
			{

				if (DefaultMappingContext)
				{
					Subsystem->RemoveMappingContext(DefaultMappingContext);
				}

				if (SeotdaMappingContext)
				{
					Subsystem->AddMappingContext(SeotdaMappingContext, 10);
				}

				bShowMouseCursor = true;
				SetInputMode(FInputModeGameAndUI());

				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Mode: SEOTDA (Mouse ON)"));
			}
			else
			{
				if (SeotdaMappingContext)
				{
					Subsystem->RemoveMappingContext(SeotdaMappingContext);
				}
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
				bShowMouseCursor = false;
				SetInputMode(FInputModeGameOnly());

				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Mode: TPS (Mouse OFF)"));
			}
		}
	}
}

void AManagerPlayerController::Client_SetSeotdaMode_Implementation(bool bEnable)
{
	SetSeotdaMode(bEnable);
}	

void AManagerPlayerController::Client_SetHandInfo_Implementation(const TArray<FString>& CardNames)
{
	MyHandNames = CardNames;
	UpdateCardDisplay();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("Received Hand Info from Server!"));

		for (const FString& Name : CardNames)
		{
			FString DebugMsg = TEXT("Client Received Card: ") + Name;
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
		}
	}
}

bool AManagerPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

//ĳ���Ϳ��� ����
void AManagerPlayerController::Input_Move(const FInputActionValue& Value)
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		MyChar->Move(Value);
	}
}

void AManagerPlayerController::Input_Look(const FInputActionValue& Value)
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		MyChar->Look(Value);
	}
}

void AManagerPlayerController::Input_Jump()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Jump")));
		}
	}
}

void AManagerPlayerController::Input_StartFire()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		MyChar->GetEquippedGun()->Setting->StartLoopFire();
	}
}
void AManagerPlayerController::Input_StopFire()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		MyChar->GetEquippedGun()->Setting->StopLoopFire();
	}
}
void AManagerPlayerController::Input_Aim()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
		}
	}
}

void AManagerPlayerController::Input_AimEnd()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->CancelAbilitiesWithTag(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim"))));
		}
	}
}

void AManagerPlayerController::Input_PickUp()
{
	/*if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.PickUp")));
		}
	}*/

	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			
			// GA_PickUp�� ������ GA_Interaction�� �����ϵ��� �±� ����
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Interaction.Interact")));
		}
	}
}

void AManagerPlayerController::Input_Drop()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Drop")));
		}
	}
}

void AManagerPlayerController::Input_Throw()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Throw")));
		}
	}
}

void AManagerPlayerController::Input_Skill00()
{
	if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
	{
		if (UCustomASC* ASC = MyChar->GetCustomASC())
		{
			ASC->TryActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Skill00")));
		}
	}
}

void AManagerPlayerController::Input_Check()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[1] Check Pressed"));
	}

	Server_SendAction(FName("Check"));
}

void AManagerPlayerController::Input_Call()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[2] Call Pressed"));
	}

	Server_SendAction(FName("Call"));
}

void AManagerPlayerController::Input_Half()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[3] Half Pressed"));
	}

	Server_SendAction(FName("Half"));
}

void AManagerPlayerController::Input_Die()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[4] Die Pressed"));
	}

	Server_SendAction(FName("Die"));
}

void AManagerPlayerController::Input_AllIn()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("[5] All-In Pressed"));
	}

	Server_SendAction(FName("AllIn"));
}


void AManagerPlayerController::Input_SelectCard1()
{
	bSelectedCards[0] = !bSelectedCards[0];
	UpdateCardDisplay();
}

void AManagerPlayerController::Input_SelectCard2()
{
	bSelectedCards[1] = !bSelectedCards[1];
	UpdateCardDisplay();
}

void AManagerPlayerController::Input_SelectCard3()
{
	bSelectedCards[2] = !bSelectedCards[2];
	UpdateCardDisplay();
}

void AManagerPlayerController::Input_ConfirmSelection()
{
	int32 Count = 0;
	TArray<int32> Indices;

	for (int32 i = 0; i < 3; ++i)
	{
		if (bSelectedCards[i])
		{
			Count++;
			Indices.Add(i);
		}
	}

	if (Count != 2)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("You must select exactly 2 cards!"));
		}
		return;
	}

	FString Command = FString::Printf(TEXT("SelectCards_%d_%d"), Indices[0], Indices[1]);

	Server_SendAction(FName(*Command));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Sending Selection to Server..."));
	}
}

void AManagerPlayerController::ToggleModeTest()
{
	bool bNewMode = !bShowMouseCursor;
	SetSeotdaMode(bNewMode);
}

void AManagerPlayerController::TPS_UI() {
	if (IsLocalPlayerController())
	{
		if (PlayerTPSUI)
		{
			PlayerHUDWidget = CreateWidget<UTpsPlayerMainHUD>(this, PlayerTPSUI);
			if (PlayerHUDWidget)
			{
				PlayerHUDWidget->AddToViewport();
				if(AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
					PlayerHUDWidget->BindCharacterState(MyChar->CharacterState);
			}
		}
		if (PlayerCardUI)
		{
			PlayerCardHUDWidget = CreateWidget<UCardPlayerMainHUD>(this, PlayerCardUI);
			if (PlayerCardHUDWidget)
			{
				PlayerCardHUDWidget->AddToViewport();
				if (AManagerCharacter* MyChar = Cast<AManagerCharacter>(GetPawn()))
					PlayerCardHUDWidget->BindCharacterState(MyChar->CharacterState);
			}
		}


	}
}

void AManagerPlayerController::UpdateCardDisplay()
{
	FString Header = TEXT("--- [ YOUR HAND (Pick 2) ] ---");
	GEngine->AddOnScreenDebugMessage(500, 10.0f, FColor::White, Header);

	for (int32 i = 0; i < 3; ++i)
	{
		FString CheckMark = bSelectedCards[i] ? TEXT("[ V ]") : TEXT("[   ]");
		FColor TextColor = bSelectedCards[i] ? FColor::Green : FColor::Red;

		FString KeyName;
		if (i == 0) KeyName = TEXT("Z");
		else if (i == 1) KeyName = TEXT("X");
		else KeyName = TEXT("C");

		FString CardName = TEXT("???");
		if (MyHandNames.IsValidIndex(i))
		{
			CardName = MyHandNames[i];
		}

		FString Msg = FString::Printf(TEXT("Key '%s' : %s %s"), *KeyName, *CardName, *CheckMark);

		GEngine->AddOnScreenDebugMessage(501 + i, 10.0f, TextColor, Msg);
	}

	GEngine->AddOnScreenDebugMessage(505, 10.0f, FColor::Yellow, TEXT("Press 'SPACE' to Confirm Selection"));
}

void AManagerPlayerController::Client_StateReset_Implementation()
{
	for (int i = 0; i < 3; i++) bSelectedCards[i] = false;

	UpdateCardDisplay();
}

bool AManagerPlayerController::Server_SendAction_Validate(FName ActionName)
{
	return true;
}

void AManagerPlayerController::Server_SendAction_Implementation(FName ActionName)
{
	if (AManagerGameMode* GM = Cast<AManagerGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnPlayerAction(this, ActionName);
	}
}