// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "ManagerPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AManagerPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	UFUNCTION(BlueprintCallable, Category = "Seotda")
	void SetSeotdaMode(bool bEnable);

	UFUNCTION(Client, Reliable)
	void Client_SetHandInfo(const TArray<FString>& CardNames);

	UFUNCTION(Client, Reliable)
	void Client_StateReset();

protected:
	bool bSelectedCards[3] = { false, false, false };
	TArray<FString> MyHandNames;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Seotda")
	UInputMappingContext* SeotdaMappingContext;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Look;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|TPS")
	UInputAction* IA_Aim;

	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* IA_PickUp;

	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* IA_Drop;

	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* IA_Throw;

	UPROPERTY(EditAnywhere, Category = "Input|TPS")
	UInputAction* IA_Skill00;

	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_Check;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_Call;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_Half;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_Die;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_AllIn;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_SelectCard1;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_SelectCard2;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_SelectCard3;
	UPROPERTY(EditAnywhere, Category = "Input|Seotda")
	UInputAction* IA_ConfirmSelection;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_StartFire();
	void Input_StopFire();
	void Input_Aim();
	void Input_AimEnd();
	void Input_PickUp();
	void Input_Drop();
	void Input_Throw();
	void Input_Skill00();

	void Input_Check();
	void Input_Call();
	void Input_Half();
	void Input_Die();
	void Input_AllIn();
	void Input_SelectCard1();
	void Input_SelectCard2();
	void Input_SelectCard3();
	void Input_ConfirmSelection();

public:
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendAction(FName ActionName);
	void ToggleModeTest();
	void UpdateCardDisplay();
protected:
	/** Gameplay initialization */
	virtual void BeginPlay() override;

protected:
	//�÷��̾� UI
	
	UPROPERTY(EditDefaultsOnly, Category = UI)
	TSubclassOf<class UTpsPlayerMainHUD> PlayerTPSUI;
	UPROPERTY(Transient)
	UTpsPlayerMainHUD* PlayerHUDWidget = nullptr;


	UPROPERTY(EditDefaultsOnly, Category = UI)
	TSubclassOf<class UCardPlayerMainHUD> PlayerCardUI;
	UPROPERTY(Transient)
	UCardPlayerMainHUD* PlayerCardHUDWidget = nullptr;

	virtual void TPS_UI();
};
