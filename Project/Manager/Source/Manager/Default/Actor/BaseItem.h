// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/InGame/Card/Data/SeotdaTypes.h"
#include "BaseItem.generated.h"

class AManagerCharacter;

UENUM(Blueprintable, BlueprintType)
enum class EPickableType : uint8
{
	Item    UMETA(DisplayName = "Consumable/Equipment"),
	Skill   UMETA(DisplayName = "Skill Book"),
	Card    UMETA(DisplayName = "Collection Card")
};

UCLASS(BlueprintType)
class MANAGER_API ABaseItem : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABaseItem();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void OnPickedUp(AManagerCharacter* Player);
	virtual void OnDropped(FVector DropLocation);

	EPickableType GetItemType() const { return ItemType; }
	ECardMonth GetCardMonth() const { return CardMonth; }
	bool IsKwangCard() const { return bIsKwangCard; }
protected:
	// --- 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// --- 공통 데이터 ---

	// 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickable Data")
	FString ItemName;

	// 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickable Data")
	UTexture2D* Icon;

	// 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickable Data")
	FString Description;

	// 종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickable Data")
	EPickableType ItemType;

	// 섯다 카드 월
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda Card")
	ECardMonth CardMonth = ECardMonth::None;

	// 광 카드 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seotda Card")
	bool bIsKwangCard = false;
};
