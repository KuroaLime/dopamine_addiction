// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "UIHandler.generated.h"

class APlayerController;
class UUserWidget;
class UCharacterStateComponent;

UCLASS( Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UUIHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UUIHandler();

	virtual void BeginPlay() override;


protected:
	UPROPERTY()
	APlayerController* OwnerController = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PlayerUI;

	UPROPERTY(Transient)
	UUserWidget* PlayerWidget = nullptr;

	
	virtual void CreateHUD();
	virtual void ShowHUD();
	virtual void HideHUD();

	void SetWidgetVisibility(ESlateVisibility Visibility);

	UCharacterStateComponent* ResolveOwnerCharacterState() const;

	UPROPERTY()
	TObjectPtr<UUserWidget> ManagedWidget=nullptr;
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void UIActivate();

	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void UIDeactivate();

	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void UIToggle();

	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void SetUITimer(int32 time);

	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void SetIsFocusable(bool isFocus);

	UUserWidget* GetWidget() {
		return PlayerWidget;
	};

	//임시방편
	UUserWidget* GetManagedWidget() const { return ManagedWidget; }
};
