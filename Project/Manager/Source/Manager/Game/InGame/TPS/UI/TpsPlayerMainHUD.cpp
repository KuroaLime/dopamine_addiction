#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/WidgetAnimation.h"
#include "Camera/CameraComponent.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/MainPlayerState.h"      
#include "Game/InGame/MainCharacter.h"
#include "Game/InGame/TPS/UI/CRoundandTimerWidget.h"
#include "Game/InGame/MainPlayerController.h"

const FName UTpsPlayerMainHUD::HPRadialWipeParamName(TEXT("Radial_wipe"));
const FName UTpsPlayerMainHUD::LvLinearWipeParamName(TEXT("Linear_wipe"));

void UTpsPlayerMainHUD::BindCharacterState(UCharacterStateComponent* NewCharacterState)
{
    CurrentCharacterState = NewCharacterState;
    NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);
    NewCharacterState->OnLEVELChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevel);
    StaticUI();
    UpdateHPWidget();
    UpdateLevel();
    bNeedPlayerStateBind = true;
    TryBindPlayerState();

    if (CRoundandTimer_UI)
    {
        CRoundandTimer_UI->BindCharacterState(NewCharacterState);
    }
    UpdateCRoundandTimer_UI();

    DiscradSelectionIndex = -1;
}

void UTpsPlayerMainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    HP_Image = Cast<UImage>(GetWidgetFromName(TEXT("HP_Circle")));
    if (HP_Image)
    {
        UMaterialInterface* BaseMaterial = HP_Image->GetDynamicMaterial();
        HPCircleMID = Cast<UMaterialInstanceDynamic>(BaseMaterial);

        if (HPCircleMID)
        {
            HPCircleMID->SetScalarParameterValue(HPRadialWipeParamName, 0.f);
        }
    }
    MaxHPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("MaxHP_Text")));
    HPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("HP_Text")));

    WEAPONImage = Cast<UImage>(GetWidgetFromName(TEXT("Weapon_Icon")));

    CardImage[0] = Cast<UImage>(GetWidgetFromName(TEXT("Card00")));
    CardImage[1] = Cast<UImage>(GetWidgetFromName(TEXT("Card01")));
    CardImage[2] = Cast<UImage>(GetWidgetFromName(TEXT("Card02")));

    Lv_Image[0] = Cast<UImage>(GetWidgetFromName(TEXT("LvHealth")));
    Lv_Image[1] = Cast<UImage>(GetWidgetFromName(TEXT("LvHealthRegen")));
    Lv_Image[2] = Cast<UImage>(GetWidgetFromName(TEXT("LvMoveSpeed")));
    Lv_Image[3] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponDamage")));
    Lv_Image[4] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponFireRate")));
    Lv_Image[5] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponRange")));
    Lv_Image[6] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponMagazine")));
    Lv_Image[7] = Cast<UImage>(GetWidgetFromName(TEXT("LvWeaponReload")));
    for (int32 i = 0; i < LvTotalNumber; ++i)
    {
        UMaterialInterface* BaseMaterial = Lv_Image[i]->GetDynamicMaterial();
        LvLinearMID[i] = Cast<UMaterialInstanceDynamic>(BaseMaterial);

        if (LvLinearMID[i])
        {
            LvLinearMID[i]->SetScalarParameterValue(LvLinearWipeParamName, 0.f);
        }
    }


    Aim_Image = Cast<UImage>(GetWidgetFromName(TEXT("Aim_Icon")));

    TPS_Compass = Cast<UUserWidget>(GetWidgetFromName(TEXT("Compass")));

    for (int32 i = 0; i < CardTotalNumber; i++)
    {
        Cards[i] = ECardID::None;
    }
    WEAPONMAXTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("WEAPONMAXTxt")));
    WEAPONCountxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("WEAPONCountxt")));

}

void UTpsPlayerMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateCompass();
    UpdateWeaponCountWidget();
    if (bNeedPlayerStateBind)
        TryBindPlayerState();
    UpdateNameWidget();
}

void UTpsPlayerMainHUD::StaticUI()
{
    UpdateWeaponIconWidget();
}

void UTpsPlayerMainHUD::UpdateHPWidget()
{
    if (CurrentCharacterState.IsValid())
    {
        if (HP_Image)      HPCircleMID->SetScalarParameterValue(HPRadialWipeParamName, CurrentCharacterState->GetHPRatio());
        if (HPTxt)         HPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetCurrentHP()));
        if (MaxHPTxt)      MaxHPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
    }
}

void UTpsPlayerMainHUD::UpdateNameWidget()
{
    if (!NAMETxt)
    {
        return;
    }

    if (CachedPlayerState.IsValid())
    {
        NAMETxt->SetText(FText::FromString(CachedPlayerState->GetPlayerName()));
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    AMainPlayerState* PS = PC ? PC->GetPlayerState<AMainPlayerState>() : nullptr;
    if (PS)
    {
        NAMETxt->SetText(FText::FromString(PS->GetPlayerName()));
        return;
    }

    if (CurrentCharacterState.IsValid() && CurrentCharacterState->GetOwner())
    {
        NAMETxt->SetText(FText::FromString(CurrentCharacterState->GetOwner()->GetName()));
    }
}

void UTpsPlayerMainHUD::OnOwnedCardsChanged(const TArray<FOwnedCardInfo>& NewCards)
{
    for (int32 i = 0; i < CardTotalNumber; i++)
    {
        if (!CardImage[i]) continue;

        if (NewCards.IsValidIndex(i))
        {
            Cards[i] = NewCards[i].CardID;
        }
        else
        {
            Cards[i] = ECardID::None;

            UTexture2D** FoundTexture = CardTextureMap.Find(Cards[i]);
            if (FoundTexture && *FoundTexture)
            {
                CardImage[i]->SetBrushFromTexture(*FoundTexture);
                CardImage[i]->SetVisibility(ESlateVisibility::Visible);
            }
        }
    }

    TriggerCardFlip();
}

void UTpsPlayerMainHUD::TryBindPlayerState()
{
    if (CachedPlayerState.IsValid()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    CachedPlayerState = PS;
    PS->OnOwnedCardsChangedNative.AddUObject(this, &UTpsPlayerMainHUD::OnOwnedCardsChanged);

    // 플레이어 데이터, 누적 업그레이드 변경 감지
    PS->OnPlayerDataChangedNative.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevel);
    PS->OnAccumulatedUpgradesChangedNative.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevel);

    if (PS->OwnedCards.Num() > 0)
        OnOwnedCardsChanged(PS->OwnedCards);

    UpdateNameWidget();
    bNeedPlayerStateBind = false;
}

void UTpsPlayerMainHUD::TriggerCardFlip()
{
    PlayAnimation(CardFlipAnim);
}

void UTpsPlayerMainHUD::OnCardFlipMidpoint()
{
    for(int32 i = 0; i < CardTotalNumber; i++)
    {
        UTexture2D** FoundTexture = CardTextureMap.Find(Cards[i]);
        if (FoundTexture && *FoundTexture)
        {
            CardImage[i]->SetBrushFromTexture(*FoundTexture);
            CardImage[i]->SetVisibility(ESlateVisibility::Visible);
        }
    }
}

void UTpsPlayerMainHUD::UpdateCardWidget()
{
    if (CachedPlayerState.IsValid())
    {
        OnOwnedCardsChanged(CachedPlayerState->OwnedCards);
    }
}

void UTpsPlayerMainHUD::UpdateWeaponIconWidget()
{
    if (CurrentCharacterState.IsValid())
    {
        if (WEAPONImage) WEAPONImage->SetBrushFromTexture(UsingWeapon_Images);
    }
}

void UTpsPlayerMainHUD::UpdateWeaponCountWidget()
{
    AMainCharacter* Character = Cast<AMainCharacter>(GetOwningPlayerPawn());
    if (Character && WEAPONCountxt)
    {
        AWeapon* Weapon = Character->GetEquippedGun();
        if (Weapon && Weapon->Setting)
        {
            WEAPONCountxt->SetText(FText::AsNumber(Weapon->Setting->GetCurrentAmmo()));
        }
        else
        {
            WEAPONCountxt->SetText(FText::AsNumber(0));
        }
    }
    if (CurrentCharacterState.IsValid() && WEAPONMAXTxt)
    {
        WEAPONMAXTxt->SetText(FText::AsNumber(CurrentCharacterState->GetCurrentAmmoCount()));
    }
}

void UTpsPlayerMainHUD::UpdateAim()
{
    if (Aim_Image) Aim_Image->SetBrushFromTexture(Aim_Images);
}

void UTpsPlayerMainHUD::UpdateLevel()
{
    if (CachedPlayerState.IsValid())
    {
        const FAccumulatedUpgrades& Upgrades = CachedPlayerState->GetAccumulatedUpgrades();

        // 각 레벨 아이콘 업데이트
        // 0: Health, 1: HealthRegen, 2: MoveSpeed, 3: WeaponDamage, 4: FireRate, 5: Range, 6: Magazine, 7: Reload
        if (Lv_Image[0]) LvLinearMID[0]->SetScalarParameterValue(LvLinearWipeParamName, (CachedPlayerState->PlayerData.LvHealth + Upgrades.LvHealth) * 0.2f);
        if (Lv_Image[1]) LvLinearMID[1]->SetScalarParameterValue(LvLinearWipeParamName, (CachedPlayerState->PlayerData.LvHealthRegeneration + Upgrades.LvHealthRegen) * 0.2f);
        if (Lv_Image[2]) LvLinearMID[2]->SetScalarParameterValue(LvLinearWipeParamName, (CachedPlayerState->PlayerData.LvMovementSpeed + Upgrades.LvMoveSpeed) * 0.2f);
        if (Lv_Image[3]) LvLinearMID[3]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponDamage * 0.2f);
        if (Lv_Image[4]) LvLinearMID[4]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponFireRate * 0.2f);
        if (Lv_Image[5]) LvLinearMID[5]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponRange * 0.2f);
        if (Lv_Image[6]) LvLinearMID[6]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponMagazine * 0.2f);
        if (Lv_Image[7]) LvLinearMID[7]->SetScalarParameterValue(LvLinearWipeParamName, Upgrades.LvWeaponReload * 0.2f);
    }
}

void UTpsPlayerMainHUD::ChangeCompassSize(float ZRotation, UImage* PSU_Compass)
{
    if (!PSU_Compass) return;

    UCanvasPanelSlot* CompassSlot = Cast<UCanvasPanelSlot>(PSU_Compass->Slot);
    if (CompassSlot)
        CompassSlot->SetPosition(FVector2D((ZRotation * -1.0f) * (PSU_Compass->GetDesiredSize().X / 360.0f), 0.0f));
}

void UTpsPlayerMainHUD::UpdateCompass()
{
    if (!CurrentCharacterState.IsValid()) return;

    UImage* PSU_Compass = Cast<UImage>(TPS_Compass->GetWidgetFromName(TEXT("IMG_CompassImage")));
    APlayerController* PC = GetOwningPlayer();

    if (PC)
    {
        IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(PC->GetPawn());
        if (Owner && Owner->GetFollowCameraComponent())
        {
            float CameraYaw = Owner->GetFollowCameraComponent()->GetComponentRotation().Yaw;
            if (PSU_Compass)
                ChangeCompassSize(CameraYaw, PSU_Compass);
        }
    }
}
void UTpsPlayerMainHUD::UpdateCRoundandTimer_UI() {
   // CRoundandTimer_UI->UpdateTimer_TextImage(1);
}

void UTpsPlayerMainHUD::CycleDiscardSelection()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    if (PS->OwnedCards.Num() > 0 )
    {
        if (PS->OwnedCards.Num() > DiscradSelectionIndex+1) DiscradSelectionIndex++;
        else DiscradSelectionIndex = 0;
    }
    else DiscradSelectionIndex = -1;


}

void UTpsPlayerMainHUD::ConfirmDiscardSelectedCard()
{
    AMainPlayerController* PC = Cast<AMainPlayerController>(GetOwningPlayer());
    if (PC && DiscradSelectionIndex != -1)
    {
        AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
        if (!PS) return;
        if (PS->OwnedCards.IsValidIndex(DiscradSelectionIndex))
            PC->Server_RequestDiscardCard(PS->OwnedCards[DiscradSelectionIndex].CardInstanceId);
    }
    DiscradSelectionIndex = -1;
}
