#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Default/Data/CharacterStateComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

#include "Animation/WidgetAnimation.h"
#include "Camera/CameraComponent.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Game/InGame/MainPlayerState.h"          // �߰�

void UTpsPlayerMainHUD::BindCharacterState(UCharacterStateComponent* NewCharacterState)
{
    CurrentCharacterState = NewCharacterState;
    NewCharacterState->OnHPChanged.AddUObject(this, &UTpsPlayerMainHUD::UpdateHPWidget);

    StaticUI();
    UpdateHPWidget();
    bNeedPlayerStateBind = true;
    TryBindPlayerState();
}

void UTpsPlayerMainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    HPProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HP_Bar")));
    MaxHPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("MaxHP_Text")));
    HPTxt = Cast<UTextBlock>(GetWidgetFromName(TEXT("HP_Text")));

    WEAPONImage = Cast<UImage>(GetWidgetFromName(TEXT("Weapon_Icon")));

    CardImage[0] = Cast<UImage>(GetWidgetFromName(TEXT("Card00")));
    CardImage[1] = Cast<UImage>(GetWidgetFromName(TEXT("Card01")));
    CardImage[2] = Cast<UImage>(GetWidgetFromName(TEXT("Card02")));

    TPS_Compass = Cast<UUserWidget>(GetWidgetFromName(TEXT("Compass")));

    for (int32 i = 0; i < CardTotalNumber; i++)
    {
        Cards[i] = ECardID::None;
    }
}

void UTpsPlayerMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateCompass();
    if (bNeedPlayerStateBind)
        TryBindPlayerState();
}

void UTpsPlayerMainHUD::StaticUI()
{
    UpdateWeaponIconWidget();
}

void UTpsPlayerMainHUD::UpdateHPWidget()
{
    if (CurrentCharacterState.IsValid())
    {
        if (HPProgressBar) HPProgressBar->SetPercent(CurrentCharacterState->GetHPRatio());
        if (HPTxt)         HPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetCurrentHP()));
        if (MaxHPTxt)      MaxHPTxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
    }
}

void UTpsPlayerMainHUD::UpdateNameWidget()
{
    if (CurrentCharacterState.IsValid())
    {
        if (NAMETxt) NAMETxt->SetText(FText::FromString(CurrentCharacterState->GetOwner()->GetName()));
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
    if (CachedPlayerState.IsValid()) return; // �̹� ��ϵ�

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    AMainPlayerState* PS = PC->GetPlayerState<AMainPlayerState>();
    if (!PS) return; // ���� ���ø����̼� �� �� �� Tick���� ��õ�

    CachedPlayerState = PS;
    PS->OnOwnedCardsChangedNative.AddUObject(this, &UTpsPlayerMainHUD::OnOwnedCardsChanged);

    if (PS->OwnedCards.Num() > 0)
        OnOwnedCardsChanged(PS->OwnedCards);

    bNeedPlayerStateBind = false; // ���� �� ��õ� �ߴ�
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
    if (CurrentCharacterState.IsValid())
    {
        if (WEAPONMaxTxt)  WEAPONMaxTxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
        if (WEAPONCountxt) WEAPONCountxt->SetText(FText::AsNumber(CurrentCharacterState->GetMaxHP()));
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
