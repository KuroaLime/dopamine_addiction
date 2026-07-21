// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/UI/WidgetParent.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

#include "GroupOwnedCard.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API UGroupOwnedCard : public UWidgetParent
{
	GENERATED_BODY()
private:
	UPROPERTY(meta = (BindWidget))
	class UImage* Background = nullptr;


	UPROPERTY(meta = (BindWidget))
	class  UTextBlock* Title = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Info = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard00 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard01 = nullptr;
	UPROPERTY(meta = (BindWidget))
	class UOwnedCard* BP_OwnedCard02 = nullptr;

protected:
	virtual void NativeConstruct() override;

	void OnPlayerCardsChanged(const TArray<FOwnedCardInfo>& NewCards);

	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category ="UI")
	TObjectPtr<class UCardTextureSet> CardTextures;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	class UWidgetAnimation* CardFlipAnim = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Card")
	void OnCardFlipMidpoint();

protected:
	// 매 프레임 검사하기 위한 틱 함수 오버라이드
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	// 바인딩 대기용 변수들
	TWeakObjectPtr<class AMainPlayerState> CachedPlayerState;
	bool bNeedPlayerStateBind = true;
	// 바인딩을 재시도할 함수
	void TryBindPlayerState();

	TArray<FOwnedCardInfo> PendingCards;
};
