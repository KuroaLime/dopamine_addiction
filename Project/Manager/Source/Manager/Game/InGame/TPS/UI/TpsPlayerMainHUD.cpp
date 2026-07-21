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

    // BindWidget으로 자동 해석된 포인터를, 인덱스 순회가 필요한 로직을 위해 편의 배열에 채워 넣는다.
    CardImage[0] = Card00;
    CardImage[1] = Card01;
    CardImage[2] = Card02;

    CardHighlightImage[0] = CardHighlight00;
    CardHighlightImage[1] = CardHighlight01;
    CardHighlightImage[2] = CardHighlight02;
    UpdateCardSelectionHighlight();

    if (HitMarker) HitMarker->SetVisibility(ESlateVisibility::Collapsed);

    Lv_Image[0] = LvHealth;
    Lv_Image[1] = LvHealthRegen;
    Lv_Image[2] = LvMoveSpeed;
    Lv_Image[3] = LvWeaponDamage;
    Lv_Image[4] = LvWeaponFireRate;
    Lv_Image[5] = LvWeaponRange;
    Lv_Image[6] = LvWeaponMagazine;
    Lv_Image[7] = LvWeaponReload;

    LvText[0] = LvHealthText;
    LvText[1] = LvHealthRegenText;
    LvText[2] = LvMoveSpeedText;
    LvText[3] = LvWeaponDamageText;
    LvText[4] = LvWeaponFireRateText;
    LvText[5] = LvWeaponRangeText;
    LvText[6] = LvWeaponMagazineText;
    LvText[7] = LvWeaponReloadText;

    for (int32 i = 0; i < LvTotalNumber; ++i)
    {
        if (!Lv_Image[i]) continue;

        UMaterialInterface* BaseMaterial = Lv_Image[i]->GetDynamicMaterial();
        LvLinearMID[i] = Cast<UMaterialInstanceDynamic>(BaseMaterial);

        if (LvLinearMID[i])
        {
            LvLinearMID[i]->SetScalarParameterValue(LvLinearWipeParamName, 0.f);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] AimBind: Up=%d(Vis=%d) Down=%d(Vis=%d) Left=%d(Vis=%d) Right=%d(Vis=%d)"),
        AimDash_Up != nullptr, AimDash_Up ? (int32)AimDash_Up->GetVisibility() : -1,
        AimDash_Down != nullptr, AimDash_Down ? (int32)AimDash_Down->GetVisibility() : -1,
        AimDash_Left != nullptr, AimDash_Left ? (int32)AimDash_Left->GetVisibility() : -1,
        AimDash_Right != nullptr, AimDash_Right ? (int32)AimDash_Right->GetVisibility() : -1);

    // 디자이너에서 기본값이 Collapsed/Hidden으로 되어 있으면 RenderTranslation을 줘도 안 보이므로,
    // 처음부터 확실하게 보이는 상태로 강제한다.
    if (AimDash_Up)    AimDash_Up->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (AimDash_Down)  AimDash_Down->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (AimDash_Left)  AimDash_Left->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (AimDash_Right) AimDash_Right->SetVisibility(ESlateVisibility::HitTestInvisible);

    for (int32 i = 0; i < CardTotalNumber; i++)
    {
        Cards[i] = ECardID::None;
    }
}

void UTpsPlayerMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateCompass();
    UpdateWeaponCountWidget();
    UpdateAim(InDeltaTime);
    if (bNeedPlayerStateBind)
        TryBindPlayerState();
    UpdateNameWidget();
}

void UTpsPlayerMainHUD::StaticUI()
{
}

void UTpsPlayerMainHUD::UpdateHPWidget()
{
    if (CurrentCharacterState.IsValid())
    {
        if (HP_Bar)        HP_Bar->SetPercent(CurrentCharacterState->GetHPRatio());
        if (HP_Text)       HP_Text->SetText(FText::AsNumber(CurrentCharacterState->GetCurrentHP()));
        if (MaxHP_Text)    MaxHP_Text->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
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

void UTpsPlayerMainHUD::UpdateAim(float DeltaTime)
{
    if (Aim_Icon) Aim_Icon->SetBrushFromTexture(Aim_Images);

    // 무기별 실제 블룸 각도(도)를 픽셀로 환산한 목표치. 무기마다 MaxBloomAngle이 다르므로
    // 퍼지는 정도(끝까지 벌어졌을 때 거리)도 무기마다 자연히 달라진다.
    float TargetOffset = 0.f;
    AMainCharacter* Character = Cast<AMainCharacter>(GetOwningPlayerPawn());
    AWeapon* Weapon = Character ? Character->GetEquippedGun() : nullptr;
    if (Weapon && Weapon->Setting)
    {
        constexpr float PixelsPerBloomDegree = 6.f; // 1도당 픽셀 거리. 취향껏 조정.
        TargetOffset = Weapon->Setting->GetCurrentBloomDegrees() * PixelsPerBloomDegree;
    }
    else
    {
        AimDebugLogAccumulator += DeltaTime;
        if (AimDebugLogAccumulator >= 0.5f)
        {
            AimDebugLogAccumulator = 0.f;
            UE_LOG(LogTemp, Warning, TEXT("[DS] UpdateAim: 조회 실패 Character=%d Weapon=%d Setting=%d"),
                Character != nullptr, Weapon != nullptr, (Weapon && Weapon->Setting));
        }
    }

    // 목표치로 순간이동하지 않고 매 프레임 부드럽게 따라가게 한다(끊기듯 튀거나 훅 줄어드는 느낌 방지).
    constexpr float InterpSpeed = 10.f;
    CurrentDisplayedAimOffset = FMath::FInterpTo(CurrentDisplayedAimOffset, TargetOffset, DeltaTime, InterpSpeed);

    if (AimDash_Up)    AimDash_Up->SetRenderTranslation(FVector2D(0.f, -CurrentDisplayedAimOffset));
    if (AimDash_Down)  AimDash_Down->SetRenderTranslation(FVector2D(0.f, CurrentDisplayedAimOffset));
    if (AimDash_Left)  AimDash_Left->SetRenderTranslation(FVector2D(-CurrentDisplayedAimOffset, 0.f));
    if (AimDash_Right) AimDash_Right->SetRenderTranslation(FVector2D(CurrentDisplayedAimOffset, 0.f));
}

void UTpsPlayerMainHUD::UpdateLevel()
{
    if (CachedPlayerState.IsValid())
    {
        const FAccumulatedUpgrades& Upgrades = CachedPlayerState->GetAccumulatedUpgrades();

        // 0: Health, 1: HealthRegen, 2: MoveSpeed, 3: WeaponDamage, 4: FireRate, 5: Range, 6: Magazine, 7: Reload
        const int32 Levels[LvTotalNumber] = {
            CachedPlayerState->PlayerData.LvHealth + Upgrades.LvHealth,
            CachedPlayerState->PlayerData.LvHealthRegeneration + Upgrades.LvHealthRegen,
            CachedPlayerState->PlayerData.LvMovementSpeed + Upgrades.LvMoveSpeed,
            Upgrades.LvWeaponDamage,
            Upgrades.LvWeaponFireRate,
            Upgrades.LvWeaponRange,
            Upgrades.LvWeaponMagazine,
            Upgrades.LvWeaponReload
        };

        for (int32 i = 0; i < LvTotalNumber; ++i)
        {
            if (Lv_Image[i] && LvLinearMID[i])
            {
                LvLinearMID[i]->SetScalarParameterValue(LvLinearWipeParamName, Levels[i] * 0.2f);
            }
            if (LvText[i])
            {
                LvText[i]->SetText(FText::FromString(FString::Printf(TEXT("LV. %d"), Levels[i])));
            }
        }
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
    if (!CurrentCharacterState.IsValid() || !Compass) return;

    UImage* PSU_Compass = Cast<UImage>(Compass->GetWidgetFromName(TEXT("IMG_CompassImage")));
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

    UpdateCardSelectionHighlight();
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
    UpdateCardSelectionHighlight();
}

void UTpsPlayerMainHUD::UpdateCardSelectionHighlight()
{
    for (int32 i = 0; i < CardTotalNumber; i++)
    {
        if (!CardHighlightImage[i]) continue;

        CardHighlightImage[i]->SetVisibility(i == DiscradSelectionIndex
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}

void UTpsPlayerMainHUD::ShowHitMarker()
{
    if (!HitMarker) return;

    HitMarker->SetVisibility(ESlateVisibility::HitTestInvisible);

    if (UWorld* World = GetWorld())
    {
        constexpr float HitMarkerDisplaySeconds = 0.15f;
        World->GetTimerManager().SetTimer(HitMarkerTimerHandle, this, &UTpsPlayerMainHUD::HideHitMarker, HitMarkerDisplaySeconds, false);
    }
}

void UTpsPlayerMainHUD::HideHitMarker()
{
    if (HitMarker) HitMarker->SetVisibility(ESlateVisibility::Collapsed);
}
