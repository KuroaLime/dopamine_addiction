// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Protocol_Client/Protocol_D.h"
#include "LoginPlayerController.generated.h"

/**
 * 
 */

UCLASS()
class MANAGER_API ALoginPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // 게임 종료시 
	virtual void OnUnPossess() override; // 폰과의 연결 해제 시

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<class UUserWidget> LoginWidgetClass;

	UPROPERTY()
	class UUserWidget* LoginWidget;


public:
	UFUNCTION(BlueprintCallable, Category = "Network")
	void ClientLogin(const FString& id, const FString& pw);
};
