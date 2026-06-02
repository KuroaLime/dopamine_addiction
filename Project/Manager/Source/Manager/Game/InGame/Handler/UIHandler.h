// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UIHandler.generated.h"

class APlayerController;
class UUserWidget;

UCLASS( Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MANAGER_API UUIHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UUIHandler();

	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void UIActivate();

	UFUNCTION(BlueprintCallable, Category = "UI")
	virtual void UIDeactivate();

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
};
