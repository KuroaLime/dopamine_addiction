// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "TpsPlayerMainHUD.generated.h"

#define SkillTotalNumber 3
/**
 * 
 */
UCLASS()
class MANAGER_API UTpsPlayerMainHUD : public UWidgetParent
{
	GENERATED_BODY()
public:
	virtual void BindCharacterState(class UCharacterStateComponent* NewCharacterState) override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void StaticUI() override;
protected:
	void UpdatePlayerImageWidget();

	void UpdateHPWidget();

	
	void UpdateLevelWidget();
	void UpdateNameWidget();
	
	void UpdateSkillWidget();

	void UpdateWeaponIconWidget();
	void UpdateWeaponCountWidget();

	void UpdateEXPWidget();

	void ChangeCompassSize(float ZRotation, class UImage* PSU_Compass);
	void UpdateCompass();
	
private:

	//Player Image
	UPROPERTY()
	UImage* PLAYERImage = nullptr;

	//HP progress bar
	UPROPERTY()
	class UProgressBar* HPProgressBar = nullptr;
	//HP TXT
	UPROPERTY()
	class UTextBlock* HPTxt = nullptr;
	UPROPERTY()
	UTextBlock* MaxHPTxt = nullptr;

	//LEVEL
	UPROPERTY()
	UTextBlock* LEVELTxt= nullptr;
	//NAME
	UPROPERTY()
	UTextBlock* NAMETxt = nullptr;

	//SKILL Image

	UPROPERTY()
	UImage* SKILLImage[SkillTotalNumber];

	//Weapon Image
	UPROPERTY()
	UImage* WEAPONImage = nullptr;
	//Weapon Count
	UPROPERTY()
	UTextBlock* WEAPONMaxTxt = nullptr;
	UPROPERTY()
	UTextBlock* WEAPONCountxt = nullptr;

	//EXP
	UPROPERTY()
	UProgressBar* EXPProgressBar = nullptr;

	//Compass
	UPROPERTY()
	UUserWidget* TPS_Compass = nullptr;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UTexture2D* PlayerIcon_Image = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UTexture2D* UsingWeapon_Images = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TArray<UTexture2D*> Skill_Images;
};
