// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ST_LobbySlot.generated.h"
/**
 * 
 */
USTRUCT(BlueprintType)
struct FLobbySlotData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SlotIndex = -1; // 초기값 설정

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<APlayerState> PlayerState = nullptr; // 포인터 안전성 강화

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsReady = false;
};

class MANAGER_API ST_LobbySlot
{
public:
	ST_LobbySlot();
	~ST_LobbySlot();
};
