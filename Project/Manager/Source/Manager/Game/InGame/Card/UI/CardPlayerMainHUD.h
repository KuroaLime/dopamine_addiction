// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "CardPlayerMainHUD.generated.h"

#define PLS 2
/**
 * 
 */
UCLASS()
class MANAGER_API UCardPlayerMainHUD : public UWidgetParent
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;

	virtual void StaticUI() override;
protected:
	void UpdateGoldBackgroundImage();
	void UpdateHoldingGoldText(float NewGold);

	void UpdateCPlayers_State_UI();
	void UpdateCRoundandTimer_UI();
private:

	//보유 골드 UI 배경
	UPROPERTY()
	class UImage* GoldBackgroundImage;
	//보유 골드 텍스트
	UPROPERTY()
	class UTextBlock* HoldingGoldText;

	//플레이어 생존 상태 표시 UI Image
	UPROPERTY()
	class UUserWidget* CPlayersStateUI;

	//Round 타이머 및 라운드 표시 UI
	UPROPERTY()
	class UUserWidget* CRoundandTimerUI;
	

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UTexture2D* GoldBackground_Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UTexture2D* RoundAndTimer_Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TArray<class UTexture2D*> PLS_Images;


	//
	UPROPERTY(meta = (BindWidget))
	class UCRoundandTimerWidget* CRoundandTimer_UI;
};