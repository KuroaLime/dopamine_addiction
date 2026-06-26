// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/Card/SeotdaRuleService.h"

FSeotdaHandResult FSeotdaRuleService::EvaluateSeotdaHand(const FOwnedCardInfo& FirstCard, const FOwnedCardInfo& SecondCard)
{
    FSeotdaHandResult Result;
    Result.UsedCardInstanceIds.Add(FirstCard.CardInstanceId);
    Result.UsedCardInstanceIds.Add(SecondCard.CardInstanceId);

    const ECardID FirstCardID = FirstCard.CardID;
    const ECardID SecondCardID = SecondCard.CardID;

    const int32 FirstMonth = GetSeotdaCardMonth(FirstCardID);
    const int32 SecondMonth = GetSeotdaCardMonth(SecondCardID);

    if (FirstMonth <= 0 || SecondMonth <= 0)
    {
        Result.Rank = -1;
        Result.SubRank = 0;
        Result.Name = TEXT("Invalid");
        return Result;
    }

    const bool bFirstGwang = IsSeotdaGwang(FirstCardID);
    const bool bSecondGwang = IsSeotdaGwang(SecondCardID);
    const bool bBothGwang = bFirstGwang && bSecondGwang;

    auto IsYulCard = [](ECardID CardID) -> bool
    {
        switch (CardID)
        {
        case ECardID::Feb_Yul:
        case ECardID::Apr_Yul:
        case ECardID::May_Yul:
        case ECardID::Jun_Yul:
        case ECardID::Jul_Yul:
        case ECardID::Aug_Yul:
        case ECardID::Sep_Yul:
        case ECardID::Oct_Yul:
            return true;
        default:
            return false;
        }
    };

    const bool bBothYul = IsYulCard(FirstCardID) && IsYulCard(SecondCardID);

    if (bBothGwang && HasSeotdaMonths(FirstMonth, SecondMonth, 3, 8))
    {
        Result.Rank = 12000;
        Result.SubRank = 38;
        Result.Name = TEXT("SamPalGwangDdang");
        return Result;
    }

    if (bBothGwang)
    {
        Result.Rank = 11000;
        Result.SubRank = FirstMonth + SecondMonth;
        Result.Name = TEXT("GwangDdang");
        return Result;
    }

    if (FirstMonth == SecondMonth)
    {
        Result.Rank = 10000 + FirstMonth;
        Result.SubRank = FirstMonth;
        Result.Name = FString::Printf(TEXT("%dDdang"), FirstMonth);
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 3, 7))
    {
        Result.Rank = 500;
        Result.SubRank = 37;
        Result.Name = TEXT("TtaengJabi");
        Result.SpecialRule = ESeotdaSpecialRule::TtaengJabi;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 7))
    {
        Result.Rank = 500;
        Result.SubRank = 47;
        Result.Name = TEXT("AmhaengEosa");
        Result.SpecialRule = ESeotdaSpecialRule::AmhaengEosa;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 9))
    {
        Result.Rank = 400;
        Result.SubRank = 49;
        Result.Name = bBothYul ? TEXT("MeongteongguriGusa") : TEXT("Gusa");
        Result.SpecialRule = bBothYul ? ESeotdaSpecialRule::MeongteongguriGusa : ESeotdaSpecialRule::Gusa;
        Result.bForcesRedeal = true;
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 2))
    {
        Result.Rank = 9000;
        Result.SubRank = 12;
        Result.Name = TEXT("Ali");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 4))
    {
        Result.Rank = 8000;
        Result.SubRank = 14;
        Result.Name = TEXT("Doksa");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 9))
    {
        Result.Rank = 7000;
        Result.SubRank = 19;
        Result.Name = TEXT("Guping");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 1, 10))
    {
        Result.Rank = 6000;
        Result.SubRank = 110;
        Result.Name = TEXT("Jangping");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 10))
    {
        Result.Rank = 5000;
        Result.SubRank = 410;
        Result.Name = TEXT("Jangsa");
        return Result;
    }

    if (HasSeotdaMonths(FirstMonth, SecondMonth, 4, 6))
    {
        Result.Rank = 4000;
        Result.SubRank = 46;
        Result.Name = TEXT("Seryuk");
        return Result;
    }

    const int32 Gut = (FirstMonth + SecondMonth) % 10;

    if (Gut == 9)
    {
        Result.Rank = 3000;
        Result.SubRank = 9;
        Result.Name = TEXT("GapOh");
        return Result;
    }

    if (Gut == 0)
    {
        Result.Rank = 0;
        Result.SubRank = 0;
        Result.Name = TEXT("Mangtong");
        return Result;
    }

    Result.Rank = 1000 + Gut;
    Result.SubRank = Gut;
    Result.Name = FString::Printf(TEXT("%dGut"), Gut);
    return Result;
}

int32 FSeotdaRuleService::GetSeotdaCardMonth(ECardID CardID)
{
    switch (CardID)
    {
    case ECardID::Jan_Gwang:
    case ECardID::Jan_HongDdi:
        return 1;

    case ECardID::Feb_Yul:
    case ECardID::Feb_HongDdi:
        return 2;

    case ECardID::Mar_Gwang:
    case ECardID::Mar_HongDdi:
        return 3;

    case ECardID::Apr_Yul:
    case ECardID::Apr_ChoDdi:
        return 4;

    case ECardID::May_Yul:
    case ECardID::May_ChoDdi:
        return 5;

    case ECardID::Jun_Yul:
    case ECardID::Jun_CheongDdi:
        return 6;

    case ECardID::Jul_Yul:
    case ECardID::Jul_ChoDdi:
        return 7;

    case ECardID::Aug_Gwang:
    case ECardID::Aug_Yul:
        return 8;

    case ECardID::Sep_Yul:
    case ECardID::Sep_CheongDdi:
        return 9;

    case ECardID::Oct_Yul:
    case ECardID::Oct_CheongDdi:
        return 10;

    default:
        return 0;
    }
}

bool FSeotdaRuleService::IsSeotdaGwang(ECardID CardID)
{
    return CardID == ECardID::Jan_Gwang ||
        CardID == ECardID::Mar_Gwang ||
        CardID == ECardID::Aug_Gwang;
}

bool FSeotdaRuleService::HasSeotdaMonths(int32 FirstMonth, int32 SecondMonth, int32 A, int32 B)
{
    return (FirstMonth == A && SecondMonth == B) || (FirstMonth == B && SecondMonth == A);
}

int32 FSeotdaRuleService::CompareSeotdaHands(const FSeotdaHandResult& A, const FSeotdaHandResult& B)
{
    auto IsSamPalGwangDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank == 12000;
    };

    auto IsGwangDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank == 11000;
    };

    auto IsNormalDdang = [](const FSeotdaHandResult& H) -> bool
    {
        return H.Rank >= 10001 && H.Rank <= 10010;
    };

    if (IsSamPalGwangDdang(A) || IsSamPalGwangDdang(B))
    {
        if (IsSamPalGwangDdang(A) && !IsSamPalGwangDdang(B)) return 1;
        if (!IsSamPalGwangDdang(A) && IsSamPalGwangDdang(B)) return -1;
    }

    if (A.SpecialRule == ESeotdaSpecialRule::AmhaengEosa && IsGwangDdang(B))
    {
        return 1;
    }

    if (B.SpecialRule == ESeotdaSpecialRule::AmhaengEosa && IsGwangDdang(A))
    {
        return -1;
    }

    if (A.SpecialRule == ESeotdaSpecialRule::TtaengJabi && IsNormalDdang(B))
    {
        return 1;
    }

    if (B.SpecialRule == ESeotdaSpecialRule::TtaengJabi && IsNormalDdang(A))
    {
        return -1;
    }

    if (A.Rank != B.Rank)
    {
        return A.Rank > B.Rank ? 1 : -1;
    }

    if (A.SubRank != B.SubRank)
    {
        return A.SubRank > B.SubRank ? 1 : -1;
    }

    return 0;
}
