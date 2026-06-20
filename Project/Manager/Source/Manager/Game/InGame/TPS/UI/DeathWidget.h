// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/InGame/Interface/UIInterface.h"
#include "DeathWidget.generated.h"

/**
 *
 */
UCLASS()
class MANAGER_API UDeathWidget : public UUserWidget,
	public IUIInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ResponeTimeText;

	UPROPERTY(meta = (BindWidget))
	class UImage* RespawnCircle;

	UPROPERTY(meta = (BindWidget))
	class UImage* RespawnCard;

	UPROPERTY(meta = (BindWidget))
	class UImage* CardFrontImage;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	class UWidgetAnimation* CardRotateAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	class UWidgetAnimation* PetalAnim;

public:
	void UpdateTime(int32 time) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (AllowPrivateAccess = "true"))
	TArray<class UTexture2D*> CardFrontImages;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetRandomCardFrontImage();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	mutable float TargetTime = 0.f;
	mutable float PrevTargetTime = 0.f;
	mutable float DisplayTime = 0.f;
	mutable float InterpElapsed = 0.f;
	mutable float MaxTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Death|Timer")
	float ServerTickInterval = 1.f;

	UPROPERTY(EditAnywhere, Category = "Death|Anim")
	float CardAnimPlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Death|Anim")
	float PetalAnimPlayRate = 1.f;

	UPROPERTY()
	class UMaterialInstanceDynamic* RespawnCircleMID = nullptr;

	static const FName RadialWipeParamName;

	void UpdateCircle(float CurrentTime) const;
	void PlayLoopingAnimations() const;
};