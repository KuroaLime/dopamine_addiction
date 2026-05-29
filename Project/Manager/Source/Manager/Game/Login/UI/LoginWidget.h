// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginWidget.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API ULoginWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget));
	class UEditableTextBox* IDInput;

	UPROPERTY(meta = (BindWidget));
	class UEditableTextBox* PWInput;

	UPROPERTY(meta = (BindWidget));
	class UButton* LoginButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnLoginButtonClick();
};
