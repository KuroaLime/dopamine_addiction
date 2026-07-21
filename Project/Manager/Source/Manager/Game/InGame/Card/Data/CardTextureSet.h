#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "CardTextureSet.generated.h"

/** 화투 카드 앞/뒷면 텍스처 단일 등록처. 인스턴스: Content/InGame/CARD/DA_CardTextures */
UCLASS(BlueprintType)
class MANAGER_API UCardTextureSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 프로젝트 기본 카드 텍스처 세트. BP가 명시적으로 지정하지 않은 최신 위젯도 안전하게 공유한다. */
	static UCardTextureSet* LoadDefault();

	/** ECardID(1~20) → 앞면 텍스처. 20행 전부 채울 것 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TMap<ECardID, TObjectPtr<UTexture2D>> FrontTextures;

	/** 공용 뒷면 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TObjectPtr<UTexture2D> BackTexture;

	/** 앞면 조회. 미등록이면 뒷면 폴백(널 브러시 방지) */
	UFUNCTION(BlueprintPure, Category = "Card")
	UTexture2D* GetFront(ECardID CardID) const
	{
		if (const TObjectPtr<UTexture2D>* Found = FrontTextures.Find(CardID))
		{
			return *Found;
		}
		return BackTexture;
	}

	/** DeathWidget 랜덤 연출용 */
	UFUNCTION(BlueprintPure, Category = "Card")
	UTexture2D* GetRandomFront() const
	{
		if (FrontTextures.Num() == 0) return BackTexture;
		TArray<TObjectPtr<UTexture2D>> Values;
		FrontTextures.GenerateValueArray(Values);
		return Values[FMath::RandRange(0, Values.Num() - 1)];
	}
};
