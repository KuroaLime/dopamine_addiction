#pragma once

#include "CoreMinimal.h"
#include "Game/InGame/Handler/UIHandler.h"
#include "CardUIHandler.generated.h"

class USeotdaTempWidget;

UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MANAGER_API UCardUIHandler : public UUIHandler
{
GENERATED_BODY()

public:
UCardUIHandler();

virtual void BeginPlay() override;

public:
virtual void UIActivate() override;
virtual void UIDeactivate() override;
virtual void SetUITimer(int32 time) override;

protected:
virtual void CreateHUD() override;
virtual void ShowHUD() override;
virtual void HideHUD() override;

private:
UPROPERTY(Transient)
TObjectPtr<USeotdaTempWidget> SeotdaTempWidget = nullptr;
};
