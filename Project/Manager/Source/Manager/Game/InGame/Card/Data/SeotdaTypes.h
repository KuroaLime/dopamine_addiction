// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SeotdaTypes.generated.h"

// 섯다 베팅 액션. 실제 섯다 구현(ECardID + FSeotdaHandResult + UCardGameService)에서 사용한다.
// (기존의 미사용 옛 섯다 모델 EAIStyle/EMatchPhase/ECardMonth/SeotdaRank/
//  FSeotdaCard/FSeotdaPlayerInfo/FSeotdaDealer/FSeotdaTable 은 사용처가 없어 제거됨)
UENUM(BlueprintType)
enum class EBettingAction : uint8
{
    None,
    Check,
    Call,
    Quarter,
    Half,
    Ddadang,
    Pping,
    Die,
    AllIn
};

// 섯다 테이블 UI에서 "다른 플레이어" 좌석 표시용. 서버가 매 상태 변화마다 각 클라이언트에 브로드캐스트한다.
USTRUCT(BlueprintType)
struct FSeotdaOpponentInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    int32 BetMoney = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bFolded = false;

    UPROPERTY(BlueprintReadOnly)
    bool bAllIn = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsCurrentTurn = false;

    UPROPERTY(BlueprintReadOnly)
    int32 SeatIndex = 0;
};
