// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

// 섯다 족보 특수룰. 기존 AMainGameMode 내부 private 정의에서 도메인 헤더로 이전.
enum class ESeotdaSpecialRule : uint8
{
    None = 0,
    TtaengJabi,
    Gusa,
    MeongteongguriGusa,
    AmhaengEosa
};

// 섯다 패 평가 결과. Rank/SubRank로 우열을 가리고, 특수룰/재딜 여부를 함께 담는다.
struct FSeotdaHandResult
{
    int32 Rank = 0;
    int32 SubRank = 0;
    FString Name;
    TArray<int32> UsedCardInstanceIds;
    ESeotdaSpecialRule SpecialRule = ESeotdaSpecialRule::None;
    bool bForcesRedeal = false;
};

/**
 * 섯다 규칙(패 평가/우열 비교/카드 월·광 판정) 순수 로직 서비스.
 * 상태를 갖지 않으며 GameMode/GameState에 의존하지 않는다 — 입력(카드)만으로 결과를 낸다.
 * 기존 AMainGameMode의 동명 메서드 본문을 그대로 이전한 것이라 판정 결과는 동일하다.
 */
class MANAGER_API FSeotdaRuleService
{
public:
    static FSeotdaHandResult EvaluateSeotdaHand(const FOwnedCardInfo& FirstCard, const FOwnedCardInfo& SecondCard);
    static int32 CompareSeotdaHands(const FSeotdaHandResult& A, const FSeotdaHandResult& B);

    static int32 GetSeotdaCardMonth(ECardID CardID);
    static bool IsSeotdaGwang(ECardID CardID);
    static bool HasSeotdaMonths(int32 FirstMonth, int32 SecondMonth, int32 A, int32 B);
};
