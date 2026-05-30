// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Camera/CameraComponent.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"

void UTpsPlayerMainHUD::BindCharacterState(UCharacterStateComponent* NewCharacterState) {

	CurrentCharacterState = NewCharacterState;
	NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);
	NewCharacterState->OnLEVELChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevelWidget);
	StaticUI();
	UpdateHPWidget();
	UpdateLevelWidget();

}

void UTpsPlayerMainHUD::NativeConstruct() {
	Super::NativeConstruct();

	HPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HP_Bar")));
	MaxHPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("MaxHP_Text")));
	HPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("HP_Text")));
	
	//플레이어 아이콘
	PLAYERImage = Cast<UImage>(GetWidgetFromName(TEXT("Player_Icon")));
	//무기
	WEAPONImage = Cast<UImage>(GetWidgetFromName(TEXT("Weapon_Icon")));
	//스킬
	Skill_Images.SetNum(4);
	SKILLImage[0] = Cast<UImage>(GetWidgetFromName(TEXT("Skill00")));
	SKILLImage[1] = Cast<UImage>(GetWidgetFromName(TEXT("Skill01")));
	SKILLImage[2] = Cast<UImage>(GetWidgetFromName(TEXT("Skill02")));

	LEVELTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("Level_Text")));

	TPS_Compass = Cast<UUserWidget>(GetWidgetFromName(TEXT("Compass")));
	//UpdateHPWidget();
}

void UTpsPlayerMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	UpdateCompass();
}

void UTpsPlayerMainHUD::StaticUI() {
	UpdatePlayerImageWidget();
	UpdateSkillWidget();
	UpdateWeaponIconWidget();
}
void UTpsPlayerMainHUD::UpdatePlayerImageWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != PLAYERImage) PLAYERImage->SetBrushFromTexture(PlayerIcon_Image);
	}
}

//
void UTpsPlayerMainHUD::UpdateHPWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != HPProgressBar) HPProgressBar->SetPercent(CurrentCharacterState->GetHPRatio());
		if (nullptr != HPTxt) HPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetCurrentHP()));
		if (nullptr != MaxHPTxt) MaxHPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
	}
}

//
void UTpsPlayerMainHUD::UpdateLevelWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != LEVELTxt) {
			LEVELTxt->SetText(FText::AsNumber(CurrentCharacterState->GetLevel()));
		}
	}
}
void UTpsPlayerMainHUD::UpdateNameWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != NAMETxt) HPProgressBar->SetPercent(CurrentCharacterState->GetHPRatio());
	}
}

void UTpsPlayerMainHUD::UpdateSkillWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != SKILLImage)
			for (int i = 0; i < SkillTotalNumber;i++)
				SKILLImage[i]->SetBrushFromTexture(Skill_Images[i]);
	}
}

void UTpsPlayerMainHUD::UpdateWeaponIconWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != WEAPONImage) WEAPONImage->SetBrushFromTexture(UsingWeapon_Images);
	}
}
void UTpsPlayerMainHUD::UpdateWeaponCountWidget(){
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != WEAPONMaxTxt) WEAPONMaxTxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
		if (nullptr != WEAPONCountxt) WEAPONCountxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
	}
}

void UTpsPlayerMainHUD::UpdateEXPWidget() {
	if (CurrentCharacterState.IsValid()) {
		if (nullptr != EXPProgressBar) HPProgressBar->SetPercent(CurrentCharacterState->GetHPRatio());
	}
}


void UTpsPlayerMainHUD::ChangeCompassSize(float ZRotation, UImage* PSU_Compass) {
	
	if (PSU_Compass == nullptr)
		return;

	UCanvasPanelSlot* CompassSlot = Cast<UCanvasPanelSlot>(PSU_Compass->Slot);
	CompassSlot->SetPosition(FVector2D((ZRotation*(-1.0f))* (PSU_Compass->GetDesiredSize().X / 360.0f),0.0f));
}
void UTpsPlayerMainHUD::UpdateCompass() {
	if (CurrentCharacterState.IsValid()) {
		UImage* PSU_Compass = Cast<UImage>(TPS_Compass->GetWidgetFromName(TEXT("IMG_CompassImage")));
		APlayerController* PlayerController = GetOwningPlayer();

		if (PlayerController) {
			IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(PlayerController->GetPawn());
			if (Owner && Owner->GetFollowCameraComponent()) {
				float CameraYaw = Owner->GetFollowCameraComponent()->GetComponentRotation().Yaw;

				if (nullptr != PSU_Compass) {
					ChangeCompassSize(CameraYaw, PSU_Compass);
				}
			}
		}
		

	}

}
