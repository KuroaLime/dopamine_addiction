
﻿#include "Game/InGame/Card/CardUIHandler.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Game/InGame/Card/UI/SeotdaTempWidget.h"
#include "GameFramework/PlayerController.h"

UCardUIHandler::UCardUIHandler()
{
PrimaryComponentTick.bCanEverTick = true;
}

void UCardUIHandler::BeginPlay()
{

Super::BeginPlay();

UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler BeginPlay Owner=%s OwnerController=%s Local=%d"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(GetOwner()),
*GetNameSafe(OwnerController),
OwnerController && OwnerController->IsLocalPlayerController() ? 1 : 0);
}

void UCardUIHandler::UIActivate()
{
UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler UIActivate Owner=%s OwnerController=%s Widget=%s Local=%d"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(GetOwner()),
*GetNameSafe(OwnerController),
*GetNameSafe(SeotdaTempWidget),
OwnerController && OwnerController->IsLocalPlayerController() ? 1 : 0);

if (!OwnerController)
{
OwnerController = Cast<APlayerController>(GetOwner());
}

if (!SeotdaTempWidget)
{
CreateHUD();
}

ShowHUD();
}

void UCardUIHandler::UIDeactivate()
{
UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler UIDeactivate Widget=%s"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(SeotdaTempWidget));

HideHUD();
}

void UCardUIHandler::SetUITimer(int32 time)
{
Super::SetUITimer(time);
}

void UCardUIHandler::CreateHUD()
{
if (!OwnerController)
{
OwnerController = Cast<APlayerController>(GetOwner());
}

UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler CreateHUD Owner=%s OwnerController=%s Local=%d PlayerUI=%s ExistingTemp=%s"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(GetOwner()),
*GetNameSafe(OwnerController),
OwnerController && OwnerController->IsLocalPlayerController() ? 1 : 0,
*GetNameSafe(PlayerUI),
*GetNameSafe(SeotdaTempWidget));

Super::CreateHUD();

if (!OwnerController)
{
UE_LOG(LogTemp, Warning, TEXT("[CL] CardUIHandler CreateHUD Failed: OwnerController null"));
return;
}

if (!OwnerController->IsLocalPlayerController())
{
UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler CreateHUD Skip: not local controller"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"));
return;
}

if (!SeotdaTempWidget)
{
SeotdaTempWidget = CreateWidget<USeotdaTempWidget>(OwnerController, USeotdaTempWidget::StaticClass());
if (SeotdaTempWidget)
{
SeotdaTempWidget->AddToViewport(100);
SeotdaTempWidget->SetVisibility(ESlateVisibility::Collapsed);
ManagedWidget = SeotdaTempWidget;

UE_LOG(LogTemp, Warning, TEXT("[CL] CardUIHandler CreateHUD Success: SeotdaTempWidget=%s"),
*GetNameSafe(SeotdaTempWidget));

if (GEngine)
{
GEngine->AddOnScreenDebugMessage(
2026062403,
5.0f,
FColor::Green,
TEXT("[CARD UI] CardUIHandler Created SeotdaTempWidget")
);
}
}
else
{
UE_LOG(LogTemp, Error, TEXT("[CL] CardUIHandler CreateHUD Failed: CreateWidget returned null"));
}
}
}

void UCardUIHandler::ShowHUD()
{
Super::ShowHUD();

if (!OwnerController)
{
OwnerController = Cast<APlayerController>(GetOwner());
}

if (!SeotdaTempWidget)
{
CreateHUD();
}

UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler ShowHUD OwnerController=%s TempWidget=%s Local=%d"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(OwnerController),
*GetNameSafe(SeotdaTempWidget),
OwnerController && OwnerController->IsLocalPlayerController() ? 1 : 0);

if (SeotdaTempWidget)
{
SeotdaTempWidget->SetVisibility(ESlateVisibility::Visible);
SeotdaTempWidget->RefreshFromPlayerState();

if (OwnerController && OwnerController->IsLocalPlayerController())
{
OwnerController->bShowMouseCursor = true;

FInputModeGameAndUI InputMode;
InputMode.SetHideCursorDuringCapture(false);
InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
OwnerController->SetInputMode(InputMode);
}

if (GEngine && OwnerController && OwnerController->IsLocalPlayerController())
{
GEngine->AddOnScreenDebugMessage(
2026062404,
5.0f,
FColor::Green,
TEXT("[CARD UI] CardUIHandler ShowHUD")
);
}
}
}

void UCardUIHandler::HideHUD()
{
Super::HideHUD();

UE_LOG(LogTemp, Warning, TEXT("[%s] CardUIHandler HideHUD TempWidget=%s"),
GetOwner() && GetOwner()->HasAuthority() ? TEXT("SV") : TEXT("CL"),
*GetNameSafe(SeotdaTempWidget));

if (SeotdaTempWidget)
{
SeotdaTempWidget->SetVisibility(ESlateVisibility::Collapsed);
}

if (OwnerController && OwnerController->IsLocalPlayerController())
{
OwnerController->bShowMouseCursor = false;

FInputModeGameOnly InputMode;
OwnerController->SetInputMode(InputMode);
}
}
