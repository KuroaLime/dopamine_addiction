// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BetWidget.generated.h"

class UButton;

UCLASS()
class MANAGER_API UBetWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Call;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Raise;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Die;

private:
    UFUNCTION()
    void OnCallBtnClick();
    UFUNCTION()
    void OnRaiseBtnClick();
    UFUNCTION()
    void OnDieBtnClick();
};
