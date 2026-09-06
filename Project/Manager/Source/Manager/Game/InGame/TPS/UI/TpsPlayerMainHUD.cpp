#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Game/InGame/Card/Data/CardTextureSet.h"
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
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

const FName UTpsPlayerMainHUD::LvLinearWipeParamName(TEXT("Linear_wipe"));

namespace
{
    constexpr int32 MaxHudPlayerNameLength = 10;

    FString MakeHudPlayerName(const FString& PlayerName)
    {
        return PlayerName.Left(MaxHudPlayerNameLength);
    }
}

void UTpsPlayerMainHUD::BindCharacterState(UCharacterStateComponent* NewCharacterState)
{
    CurrentCharacterState = NewCharacterState;
    NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);
    NewCharacterState->OnLEVELChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateLevel);
    StaticUI();
    UpdateHPWidget();
    UpdateLevel();
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
    if (!CardTextures)
    {
        CardTextures = UCardTextureSet::LoadDefault();
    }

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

    CreateGoldDisplay();
    UpdateGoldDisplay();
}

void UTpsPlayerMainHUD::NativeDestruct()
{
    RemoveGoldDisplay();
    Super::NativeDestruct();
}

void UTpsPlayerMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateCompass();
    UpdateWeaponCountWidget();
    UpdateAim(InDeltaTime);
    UpdateNameWidget();
    UpdateGoldDisplay();
}

void UTpsPlayerMainHUD::CreateGoldDisplay()
{
    RemoveGoldDisplay();

    UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!GameViewport)
    {
        return;
    }

    GoldDisplayIconTexture = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Game/InGame/UI/T_UI_Icon_Gold.T_UI_Icon_Gold"));

    GoldDisplayIconBrush = MakeShared<FSlateBrush>();
    GoldDisplayIconBrush->DrawAs = ESlateBrushDrawType::Image;
    GoldDisplayIconBrush->SetImageSize(FVector2D(38.0f, 38.0f));
    GoldDisplayIconBrush->SetResourceObject(GoldDisplayIconTexture);

    TSharedRef<SWidget> Overlay =
        SNew(SOverlay)
        .Visibility(EVisibility::HitTestInvisible)
        + SOverlay::Slot()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(0.0f, 0.0f, 30.0f, 24.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(128.0f)
            .MinDesiredHeight(42.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(GoldDisplayIconBrush.Get())
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                [
                    SAssignNew(GoldDisplayTextWidget, STextBlock)
                    .Text(FText::FromString(TEXT("--")))
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26))
                    .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.18f, 1.0f)))
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
                    .ShadowOffset(FVector2D(2.0f, 2.0f))
                ]
            ]
        ];

    GoldDisplayOverlayWidget = Overlay;
    GameViewport->AddViewportWidgetContent(Overlay, 60);
}

void UTpsPlayerMainHUD::UpdateGoldDisplay()
{
    if (!GoldDisplayTextWidget.IsValid())
    {
        return;
    }

    AMainPlayerState* PS = CachedPlayerState.Get();
    if (!PS)
    {
        APlayerController* PC = GetOwningPlayer();
        PS = PC ? PC->GetPlayerState<AMainPlayerState>() : nullptr;
    }

    if (!PS)
    {
        if (LastDisplayedGold != -1)
        {
            LastDisplayedGold = -1;
            GoldDisplayTextWidget->SetText(FText::FromString(TEXT("--")));
        }
        return;
    }

    const int32 CurrentGold = FMath::Max(0, PS->CurPlayerData.HoldingGold);
    if (CurrentGold != LastDisplayedGold)
    {
        LastDisplayedGold = CurrentGold;
        GoldDisplayTextWidget->SetText(FText::AsNumber(CurrentGold));
    }
}

void UTpsPlayerMainHUD::RemoveGoldDisplay()
{
    if (GoldDisplayOverlayWidget.IsValid())
    {
        if (UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        {
            GameViewport->RemoveViewportWidgetContent(GoldDisplayOverlayWidget.ToSharedRef());
        }
    }

    GoldDisplayOverlayWidget.Reset();
    GoldDisplayTextWidget.Reset();
    GoldDisplayIconBrush.Reset();
    GoldDisplayIconTexture = nullptr;
    LastDisplayedGold = -1;
}

void UTpsPlayerMainHUD::StaticUI()
{
}

void UTpsPlayerMainHUD::UpdateHPWidget()
{
	if (CurrentCharacterState.IsValid())
	{
		const float HPRatio = FMath::Clamp(CurrentCharacterState->GetHPRatio(), 0.0f, 1.0f);
		const int32 CurrentHP = FMath::RoundToInt(CurrentCharacterState->GetCurrentHP());
		const int32 MaxHP = FMath::RoundToInt(CurrentCharacterState->GetMaxHP());

		if (HP_Bar)
		{
			HP_Bar->SetPercent(HPRatio);

			// Keep the lacquer-red texture intact, but make critical health read brighter.
			const FLinearColor FillTint = HPRatio <= 0.3f
				? FLinearColor(1.0f, 0.55f, 0.45f, 1.0f)
				: FLinearColor::White;
			HP_Bar->SetFillColorAndOpacity(FillTint);
		}

		if (HP_Text)
		{
			HP_Text->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentHP, MaxHP)));
		}

		if (MaxHP_Text)
		{
			MaxHP_Text->SetText(FText::GetEmpty());
			MaxHP_Text->SetVisibility(ESlateVisibility::Collapsed);
		}
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
        NAMETxt->SetText(FText::FromString(MakeHudPlayerName(CachedPlayerState->GetPlayerName())));
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    AMainPlayerState* PS = PC ? PC->GetPlayerState<AMainPlayerState>() : nullptr;
    if (PS)
    {
        NAMETxt->SetText(FText::FromString(MakeHudPlayerName(PS->GetPlayerName())));
        return;
    }

    if (CurrentCharacterState.IsValid() && CurrentCharacterState->GetOwner())
    {
        NAMETxt->SetText(FText::FromString(MakeHudPlayerName(CurrentCharacterState->GetOwner()->GetName())));
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

            UTexture2D* BackTexture = EmptyCardTexture;
            if (!BackTexture && CardTextures)
            {
                BackTexture = CardTextures->BackTexture;
            }
            if (BackTexture)
            {
                CardImage[i]->SetBrushFromTexture(BackTexture);
                CardImage[i]->SetVisibility(ESlateVisibility::Visible);
            }
        }
    }

    TriggerCardFlip();
}

void UTpsPlayerMainHUD::OnPlayerStateReady()
{
    TryBindPlayerState();
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
    UpdateLevel();
}

void UTpsPlayerMainHUD::TriggerCardFlip()
{
    PlayAnimation(CardFlipAnim);
}

void UTpsPlayerMainHUD::OnCardFlipMidpoint()
{
    for(int32 i = 0; i < CardTotalNumber; i++)
    {
        if (Cards[i] == ECardID::None || !CardTextures) continue;

        if (UTexture2D* FrontTexture = CardTextures->GetFront(Cards[i]))
        {
            CardImage[i]->SetBrushFromTexture(FrontTexture);
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
    AWeapon* Weapon = Character ? Character->GetEquippedGun() : nullptr;
    UWeaponComponent* WeaponComp = Weapon ? Weapon->Setting : nullptr;

    // 현재 탄창에 남은 탄환 수.
    if (WEAPONCountxt)
    {
        WEAPONCountxt->SetText(FText::AsNumber(WeaponComp ? WeaponComp->GetCurrentAmmo() : 0));
    }

    // 한 탄창에 들어가는 최대 탄환 수(예비탄이 아니라 탄창 용량). 예비탄은 무제한이라 표시하지 않는다.
    if (WEAPONMAXTxt)
    {
        WEAPONMAXTxt->SetText(FText::AsNumber(WeaponComp ? WeaponComp->GetMaxMagazineCapacity() : 0));
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
        // 크로스헤어는 실제 탄 원뿔과 같은 총 각도(기본 퍼짐 + 누적 블룸)를 그린다.
        // 서버 Server_ExecuteFire의 VRandCone(SpreadAngle = base + CurrentBloomAngle)와 일치.
        // 대기 상태(블룸=0)에서는 기본 퍼짐만 표시되므로 크로스헤어 = 첫 탄이 갈 수 있는 범위가 된다.
        float BaseSpreadDegrees = 0.f;
        if (Character)
        {
            IPhasePlayerStateInterface* PS = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
            IPhaseGameStateInterface* GS = GetWorld() ? Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()) : nullptr;
            if (PS && GS)
            {
                float Spread = 0.f;
                int32 PelletCount = 1;
                float TraceRadius = 0.f;
                GS->GetWeaponFireProfile(PS->GetWeaponID(), Spread, PelletCount, TraceRadius);
                BaseSpreadDegrees = Spread;
            }
        }

        const float TotalSpreadDegrees = BaseSpreadDegrees + Weapon->Setting->GetCurrentBloomDegrees();

        // 임의 상수(px/도)가 아니라 실제 탄 원뿔을 화면 픽셀 반지름으로 투영한다.
        // 서버 탄은 VRandCone(카메라 forward, TotalSpreadDegrees)로 흩어지므로,
        // 그 반각을 현재 FOV·뷰포트로 투영하면 "탄이 화면에서 벗어날 수 있는 최대 거리(px)"가 나온다.
        // 대시는 이 반지름보다 기본 간격만큼 더 바깥이라, 탄은 크로스헤어 안쪽 간격에 떨어진다.
        float FovDegrees = 90.f;
        if (APlayerController* OwningPC = GetOwningPlayer())
        {
            if (OwningPC->PlayerCameraManager)
            {
                FovDegrees = OwningPC->PlayerCameraManager->GetFOVAngle();
            }
        }

        FVector2D ViewportSize(1920.f, 1080.f);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(ViewportSize);
        }

        const float HalfFovRad = FMath::DegreesToRadians(FMath::Clamp(FovDegrees, 1.f, 170.f) * 0.5f);
        const float HalfFovTan = FMath::Tan(HalfFovRad);
        if (HalfFovTan > KINDA_SMALL_NUMBER)
        {
            // 수평 FOV 기준 투영(FieldOfView=수평). 카메라 스프레드 반각을 화면 raw 픽셀 반지름으로 환산.
            const float SpreadPixelRadius = FMath::Tan(FMath::DegreesToRadians(TotalSpreadDegrees)) / HalfFovTan * (ViewportSize.X * 0.5f);

            // SetRenderTranslation은 UMG의 DPI 스케일된 단위이므로 raw 픽셀을 DPI 스케일로 나눠 맞춘다.
            // (이 보정이 없으면 크로스헤어가 실제 탄 반지름보다 작게 벌어져 가장자리 탄이 밖으로 삐져나온다.)
            const float DpiScale = UWidgetLayoutLibrary::GetViewportScale(this);
            const float SafeDpiScale = (DpiScale > KINDA_SMALL_NUMBER) ? DpiScale : 1.f;

            // 크로스헤어는 대칭이라 이 반지름을 4방향에 동일 적용. CrosshairSpreadScale로 미세 보정.
            TargetOffset = (SpreadPixelRadius / SafeDpiScale) * CrosshairSpreadScale;
        }
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
            CachedPlayerState->PlayerData.LvHealth + FMath::RoundToInt(Upgrades.LvHealth),
            CachedPlayerState->PlayerData.LvHealthRegeneration + FMath::RoundToInt(Upgrades.LvHealthRegen),
            CachedPlayerState->PlayerData.LvMovementSpeed + FMath::RoundToInt(Upgrades.LvMoveSpeed),
            CachedPlayerState->GetWeaponStatLV(EWeaponStatType::Damage),
            CachedPlayerState->GetWeaponStatLV(EWeaponStatType::FireRate),
            CachedPlayerState->GetWeaponStatLV(EWeaponStatType::Range),
            CachedPlayerState->GetWeaponStatLV(EWeaponStatType::MagazineCapacity),
            CachedPlayerState->GetWeaponStatLV(EWeaponStatType::ReloadTime)
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
