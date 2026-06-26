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
