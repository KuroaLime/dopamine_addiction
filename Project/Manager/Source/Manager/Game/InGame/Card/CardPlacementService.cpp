// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/Card/CardPlacementService.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

TArray<ECardID> FCardPlacementService::BuildCardBundleIDs() const
{
    TArray<ECardID> CardIDs;
    CardIDs.Reserve(20);

    // 20-card Seotda deck.
    // 1?? 愿? ?띾씈
    CardIDs.Add(ECardID::Jan_Gwang);
    CardIDs.Add(ECardID::Jan_HongDdi);

    // 2?? 10?? ?띾씈
    CardIDs.Add(ECardID::Feb_Yul);
    CardIDs.Add(ECardID::Feb_HongDdi);

    // 3?? 愿? ?띾씈
    CardIDs.Add(ECardID::Mar_Gwang);
    CardIDs.Add(ECardID::Mar_HongDdi);

    // 4?? 10?? 珥덈씈
    CardIDs.Add(ECardID::Apr_Yul);
    CardIDs.Add(ECardID::Apr_ChoDdi);

    // 5?? 10?? 珥덈씈
    CardIDs.Add(ECardID::May_Yul);
    CardIDs.Add(ECardID::May_ChoDdi);

    // 6?? 10?? 泥?씈
    CardIDs.Add(ECardID::Jun_Yul);
    CardIDs.Add(ECardID::Jun_CheongDdi);

    // 7?? 10?? 珥덈씈
    CardIDs.Add(ECardID::Jul_Yul);
    CardIDs.Add(ECardID::Jul_ChoDdi);

    // 8?? 愿? 10??
    CardIDs.Add(ECardID::Aug_Gwang);
    CardIDs.Add(ECardID::Aug_Yul);

    // 9?? 10?? 泥?씈
    CardIDs.Add(ECardID::Sep_Yul);
    CardIDs.Add(ECardID::Sep_CheongDdi);

    // 10?? 10?? 泥?씈
    CardIDs.Add(ECardID::Oct_Yul);
    CardIDs.Add(ECardID::Oct_CheongDdi);

    return CardIDs;
}

void FCardPlacementService::ShuffleCardIDs(TArray<ECardID>& CardIDs) const
{
    for (int32 Index = CardIDs.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            CardIDs.Swap(Index, SwapIndex);
        }
    }
}

FVector FCardPlacementService::GetDistributedCardDropLocation(int32 Index, int32 TotalCount) const
{
    if (TotalCount <= 0)
    {
        return CardBundleDropCenter;
    }

    const float SafeExtentX = FMath::Max(1.0f, CardBundleDropExtent.X);
    const float SafeExtentY = FMath::Max(1.0f, CardBundleDropExtent.Y);
    const float Aspect = SafeExtentX / SafeExtentY;

    // 湲곗〈 諛⑹떇? 20?μ씪 ??6x4=24移몄씠 ?섏뼱 留덉?留?以꾩씠 移섏슦移????덉뿀??
    // ??諛⑹떇? 20??湲곗? 5x4??媛源앷쾶 留뚮뱾???꾩껜 ?곸뿭????怨좊Ⅴ寃?諛곗튂?쒕떎.
    const int32 RowCount = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TotalCount) / FMath::Max(0.25f, Aspect))));
    const int32 ColumnCount = FMath::Max(1, FMath::CeilToInt(static_cast<float>(TotalCount) / static_cast<float>(RowCount)));

    const int32 Row = Index / ColumnCount;
    const int32 Column = Index % ColumnCount;

    const int32 ItemsInThisRow = FMath::Min(ColumnCount, TotalCount - Row * ColumnCount);

    const float FullWidth = CardBundleDropExtent.X * 2.0f;
    const float FullHeight = CardBundleDropExtent.Y * 2.0f;

    const float CellWidth = FullWidth / static_cast<float>(ColumnCount);
    const float CellHeight = FullHeight / static_cast<float>(RowCount);

    const float MinX = CardBundleDropCenter.X - CardBundleDropExtent.X;
    const float MinY = CardBundleDropCenter.Y - CardBundleDropExtent.Y;

    // 留덉?留?以꾩씠 苑?李⑥? ?딆븘??以묒븰 ?뺣젹?섍쾶 蹂댁젙
    const float RowWidth = CellWidth * static_cast<float>(ItemsInThisRow);
    const float RowStartX = CardBundleDropCenter.X - RowWidth * 0.5f;

    const float JitterRatio = FMath::Clamp(CardBundleDropJitterRatio, 0.0f, 0.20f);
    const float JitterX = CellWidth * JitterRatio;
    const float JitterY = CellHeight * JitterRatio;

    const float X = RowStartX + (static_cast<float>(Column) + 0.5f) * CellWidth + FMath::FRandRange(-JitterX, JitterX);
    const float Y = MinY + (static_cast<float>(Row) + 0.5f) * CellHeight + FMath::FRandRange(-JitterY, JitterY);
    const float Z = CardBundleDropCenter.Z;

    return FVector(X, Y, Z);
}

int32 FCardPlacementService::GetCardIslandBalanceValue(ECardID CardID) const
{
    switch (CardID)
    {
    case ECardID::Jan_Gwang:
    case ECardID::Mar_Gwang:
    case ECardID::Aug_Gwang:
        return 9;

    case ECardID::Feb_Yul:
    case ECardID::Apr_Yul:
    case ECardID::May_Yul:
    case ECardID::Jun_Yul:
    case ECardID::Jul_Yul:
    case ECardID::Aug_Yul:
    case ECardID::Sep_Yul:
    case ECardID::Oct_Yul:
        return 6;

    case ECardID::Jan_HongDdi:
    case ECardID::Feb_HongDdi:
    case ECardID::Mar_HongDdi:
    case ECardID::Apr_ChoDdi:
    case ECardID::May_ChoDdi:
    case ECardID::Jun_CheongDdi:
    case ECardID::Jul_ChoDdi:
    case ECardID::Sep_CheongDdi:
    case ECardID::Oct_CheongDdi:
        return 5;

    default:
        return 0;
    }
}

int32 FCardPlacementService::GetCardIslandGroupBalanceValue(const TArray<ECardID>& CardIDs) const
{
    int32 TotalValue = 0;

    for (ECardID CardID : CardIDs)
    {
        TotalValue += GetCardIslandBalanceValue(CardID);
    }

    return TotalValue;
}

TArray<TArray<ECardID>> FCardPlacementService::BuildBalancedIslandCardGroups() const
{
    TArray<TArray<ECardID>> Groups;
    Groups.SetNum(4);

    // 愿?= 9, 10??= 6, ??= 5 湲곗?.
    // 紐⑤뱺 ??洹몃９??珥앺빀??30?먯씠 ?섎룄濡?怨좎젙 援ъ꽦?쒕떎.
    Groups[0].Add(ECardID::Jan_Gwang);
    Groups[0].Add(ECardID::Aug_Yul);
    Groups[0].Add(ECardID::Feb_HongDdi);
    Groups[0].Add(ECardID::May_ChoDdi);
    Groups[0].Add(ECardID::Jul_ChoDdi);

    Groups[1].Add(ECardID::Mar_Gwang);
    Groups[1].Add(ECardID::May_Yul);
    Groups[1].Add(ECardID::Jan_HongDdi);
    Groups[1].Add(ECardID::Jun_CheongDdi);
    Groups[1].Add(ECardID::Oct_CheongDdi);

    Groups[2].Add(ECardID::Aug_Gwang);
    Groups[2].Add(ECardID::Feb_Yul);
    Groups[2].Add(ECardID::Mar_HongDdi);
    Groups[2].Add(ECardID::Apr_ChoDdi);
    Groups[2].Add(ECardID::Sep_CheongDdi);

    // 愿묒씠 ?녿뒗 洹몃９? 10??5?μ쑝濡?媛移?蹂댁젙?쒕떎.
    Groups[3].Add(ECardID::Apr_Yul);
    Groups[3].Add(ECardID::Jun_Yul);
    Groups[3].Add(ECardID::Jul_Yul);
    Groups[3].Add(ECardID::Sep_Yul);
    Groups[3].Add(ECardID::Oct_Yul);

    // ?쇱슫?쒕쭏???대뼡 ?ъ씠 ?대뼡 媛移?洹몃９??諛쏅뒗吏 ?욌뒗??
    for (int32 Index = Groups.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            Groups.Swap(Index, SwapIndex);
        }
    }

    // 媛숈? ???덉쓽 移대뱶 ?꾩튂 ?쒖꽌???욌뒗??
    for (TArray<ECardID>& Group : Groups)
    {
        ShuffleCardIDs(Group);
    }

    return Groups;
}

bool FCardPlacementService::IsSeasonIslandActorName(const FString& ActorName) const
{
    return ActorName.Contains(TEXT("BPP_MAP_Summer"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Spring"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Autumn"), ESearchCase::IgnoreCase)
        || ActorName.Contains(TEXT("BPP_MAP_Winter"), ESearchCase::IgnoreCase);
}


TArray<FCardIslandDropZone> FCardPlacementService::FindCardIslandDropZones() const
{
    TArray<FCardIslandDropZone> DropZones;

    if (!World)
    {
        return DropZones;
    }

    auto GetActorSearchText = [](const AActor* Actor) -> FString
    {
        if (!IsValid(Actor))
        {
            return FString();
        }

        const FString ActorName = GetNameSafe(Actor);
        FString LabelName;
#if WITH_EDITOR
        LabelName = Actor->GetActorLabel();
#endif
        const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
        return ActorName + TEXT(" ") + LabelName + TEXT(" ") + ClassName;
    };

    auto GetSeasonIslandKey = [](const FString& SearchText, int32& OutSortOrder) -> FName
    {
        if (SearchText.Contains(TEXT("BPP_MAP_Winter"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 0;
            return FName(TEXT("Winter"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Spring"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 1;
            return FName(TEXT("Spring"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Summer"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 2;
            return FName(TEXT("Summer"));
        }
        if (SearchText.Contains(TEXT("BPP_MAP_Autumn"), ESearchCase::IgnoreCase))
        {
            OutSortOrder = 3;
            return FName(TEXT("Autumn"));
        }

        OutSortOrder = 1000;
        return NAME_None;
    };

    auto MakeZone = [](AActor* Actor, const FVector& Origin, const FVector& Extent, FName IslandKey, const FString& Source, int32 SortOrder) -> FCardIslandDropZone
    {
        FCardIslandDropZone Zone;
        Zone.ZoneActor = Actor;
        Zone.Bounds = FBox(Origin - Extent, Origin + Extent);
        Zone.Center = Origin;
        Zone.IslandKey = IslandKey;
        Zone.Source = Source;
        Zone.SortOrder = SortOrder;
        return Zone;
    };

    TArray<FCardIslandDropZone> TaggedNavAreaZones;
    TMap<FName, FCardIslandDropZone> SeasonZonesByKey;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        FVector Origin;
        FVector Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        if (Extent.X < 500.0f || Extent.Y < 500.0f)
        {
            continue;
        }

        const FString SearchText = GetActorSearchText(Actor);
        const bool bTaggedNavArea = Actor->ActorHasTag(FName(TEXT("CardIslandNavArea")));

        int32 SeasonSortOrder = 1000;
        const FName SeasonKey = GetSeasonIslandKey(SearchText, SeasonSortOrder);

        if (bTaggedNavArea)
        {
            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, Actor->GetFName(), TEXT("CardIslandNavArea"), TaggedNavAreaZones.Num());
            TaggedNavAreaZones.Add(Zone);

            UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=CardIslandNavArea Actor=%s Key=%s Center=%s Extent=%s"),
                *GetNameSafe(Actor), *Zone.IslandKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString());
        }

        if (bAutoDetectSeasonIslandActorsAsDropZones && !SeasonKey.IsNone())
        {
            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, SeasonKey, TEXT("SeasonIslandActor"), SeasonSortOrder);
            FCardIslandDropZone* ExistingZone = SeasonZonesByKey.Find(SeasonKey);
            const float NewArea = Extent.X * Extent.Y;
            const float ExistingArea = ExistingZone ? ExistingZone->Bounds.GetExtent().X * ExistingZone->Bounds.GetExtent().Y : -1.0f;

            if (!ExistingZone || NewArea > ExistingArea)
            {
                SeasonZonesByKey.Add(SeasonKey, Zone);

                UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=SeasonIslandActor Actor=%s Key=%s Center=%s Extent=%s Selected=%d"),
                    *GetNameSafe(Actor), *SeasonKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString(), 1);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=SeasonIslandActor Actor=%s Key=%s Center=%s Extent=%s Selected=%d Reason=SmallerDuplicate"),
                    *GetNameSafe(Actor), *SeasonKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString(), 0);
            }
        }
    }

    if (TaggedNavAreaZones.Num() >= CardIslandDropExpectedZoneCount)
    {
        DropZones = TaggedNavAreaZones;
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=CardIslandNavArea Count=%d"), DropZones.Num());
    }
    else if (SeasonZonesByKey.Num() > 0)
    {
        SeasonZonesByKey.GenerateValueArray(DropZones);
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=SeasonIslandActor Count=%d TaggedNavAreas=%d"),
            DropZones.Num(), TaggedNavAreaZones.Num());
    }
    else if (TaggedNavAreaZones.Num() > 0)
    {
        DropZones = TaggedNavAreaZones;
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas Mode=PartialCardIslandNavArea Count=%d"), DropZones.Num());
    }

    // 혹시 섬 액터/NavArea를 못 찾으면 기존 TriggerBox 방식으로만 fallback
    if (DropZones.Num() == 0)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!IsValid(Actor))
            {
                continue;
            }

            if (!Actor->ActorHasTag(CardIslandDropZoneTag))
            {
                continue;
            }

            FVector Origin;
            FVector Extent;
            Actor->GetActorBounds(false, Origin, Extent);

            FCardIslandDropZone Zone = MakeZone(Actor, Origin, Extent, Actor->GetFName(), TEXT("TriggerBoxFallback"), DropZones.Num());

            DropZones.Add(Zone);

            UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Candidate Source=TriggerBoxFallback Actor=%s Key=%s Center=%s Extent=%s"),
                *GetNameSafe(Actor), *Zone.IslandKey.ToString(), *Origin.ToCompactString(), *Extent.ToCompactString());
        }

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandAreas FallbackToTriggerBoxes Count=%d"), DropZones.Num());
    }

    DropZones.Sort([](const FCardIslandDropZone& A, const FCardIslandDropZone& B)
    {
        if (A.SortOrder != B.SortOrder)
        {
            return A.SortOrder < B.SortOrder;
        }

        if (!FMath::IsNearlyEqual(A.Center.Y, B.Center.Y))
        {
            return A.Center.Y < B.Center.Y;
        }

        return A.Center.X < B.Center.X;
    });

    for (int32 Index = 0; Index < DropZones.Num(); ++Index)
    {
        const FCardIslandDropZone& Zone = DropZones[Index];
        UE_LOG(LogTemp, Warning, TEXT("[DS] Card IslandArea Selected Index=%d Source=%s Key=%s Actor=%s Center=%s Extent=%s"),
            Index, *Zone.Source, *Zone.IslandKey.ToString(), *GetNameSafe(Zone.ZoneActor.Get()),
            *Zone.Center.ToCompactString(), *Zone.Bounds.GetExtent().ToCompactString());
    }

    return DropZones;
}

bool FCardPlacementService::IsCardIslandSurfaceWalkable(const FHitResult& Hit) const
{
    if (!Hit.bBlockingHit)
    {
        return false;
    }

    const float ClampedSlopeDegrees = FMath::Clamp(CardIslandMaxGroundSlopeDegrees, 0.0f, 89.0f);
    const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(ClampedSlopeDegrees));

    return Hit.ImpactNormal.Z >= MinNormalZ;
}

bool FCardPlacementService::IsCardDropLocationClear(const FVector& CandidateLocation) const
{
    if (!World)
    {
        return false;
    }

    const FVector SafeExtent(
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.X),
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.Y),
        FMath::Max(1.0f, CardIslandOverlapBoxExtent.Z));

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardIslandDropClear), false);
    QueryParams.bTraceComplex = false;

    const FCollisionShape CheckShape = FCollisionShape::MakeBox(SafeExtent);

    const bool bOverlapsBlockingObject = World->OverlapBlockingTestByChannel(
        CandidateLocation,
        FQuat::Identity,
        ECC_WorldDynamic,
        CheckShape,
        QueryParams);

    return !bOverlapsBlockingObject;
}

bool FCardPlacementService::IsFarEnoughFromIslandCards(const FVector& CandidateLocation, const TArray<FVector>& ExistingIslandLocations) const
{
    if (CardIslandMinCardDistance <= 0.0f)
    {
        return true;
    }

    const float MinDistanceSq = FMath::Square(CardIslandMinCardDistance);

    for (const FVector& ExistingLocation : ExistingIslandLocations)
    {
        if (FVector::DistSquared2D(CandidateLocation, ExistingLocation) < MinDistanceSq)
        {
            return false;
        }
    }

    return true;
}

bool FCardPlacementService::IsCardDropZSane(const FCardIslandDropZone& DropZone, float ReferenceNavZ, const FVector& Candidate) const
{
    const float CandidateNavZ = Candidate.Z - CardIslandGroundOffsetZ;
    const float MaxDelta = FMath::Max(120.0f, CardIslandMaxGroundZDelta);

    if (FMath::Abs(CandidateNavZ - ReferenceNavZ) > MaxDelta)
    {
        return false;
    }

    const float BoundsPadding = FMath::Max(120.0f, CardIslandGroundOffsetZ + 40.0f);
    if (Candidate.Z < DropZone.Bounds.Min.Z - BoundsPadding)
    {
        return false;
    }

    if (Candidate.Z > DropZone.Bounds.Max.Z + BoundsPadding)
    {
        return false;
    }

    return true;
}

bool FCardPlacementService::IsInsideNoDropZone(const FVector& Candidate) const
{
    if (!World || CardNoDropZoneTag.IsNone())
    {
        return false;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || !Actor->ActorHasTag(CardNoDropZoneTag))
        {
            continue;
        }

        FVector Origin;
        FVector Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        const FBox NoBox(Origin - Extent, Origin + Extent);
        if (NoBox.IsInsideXY(Candidate))
        {
            return true;
        }
    }

    return false;
}

bool FCardPlacementService::HasOverheadClearance(const FVector& Candidate) const
{
    if (!World)
    {
        return true;
    }

    // 보조 검증용 LineTrace: 카드 바로 위로 짧게 쏘아 머리 위가 막혀 있으면(캐노피/바위 밑) 제외한다.
    // Spawn Z 자체는 NavMesh 지면 Z 기준이므로 이 트레이스는 Z를 바꾸지 않는다.
    const float ClearHeight = FMath::Max(1.0f, CardIslandOverheadClearance);
    const FVector Start = Candidate;
    const FVector End = Candidate + FVector(0.0f, 0.0f, ClearHeight);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardIslandDropOverhead), false);
    QueryParams.bTraceComplex = false;

    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldDynamic, QueryParams);
    return !bBlocked;
}

bool FCardPlacementService::PickIslandCardDropLocation(const FCardIslandDropZone& DropZone, const TArray<FVector>& ExistingIslandLocations, int32 IslandIndex, int32 SlotIndex, FVector& OutLocation) const
{
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Reason=NoWorld"), IslandIndex, SlotIndex);
        return false;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Reason=NoNavSystem"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()));
        return false;
    }

    const FVector Extent = DropZone.Bounds.GetExtent();
    const FVector Center = DropZone.Center;

    const FVector AnchorProjectExtent(
        FMath::Max(400.0f, CardIslandNavProjectExtent.X),
        FMath::Max(400.0f, CardIslandNavProjectExtent.Y),
        FMath::Max(1200.0f, FMath::Max(CardIslandNavProjectExtent.Z, Extent.Z + 400.0f)));

    const FVector CandidateProjectExtent(
        FMath::Max(200.0f, CardIslandNavProjectExtent.X),
        FMath::Max(200.0f, CardIslandNavProjectExtent.Y),
        FMath::Max(500.0f, CardIslandNavProjectExtent.Z));

    const float MinDistance = FMath::Max(1.0f, CardIslandMinCardDistance);
    const int32 MaxAttempts = FMath::Max(160, CardIslandDropMaxAttemptsPerCard);
    const float VisibleRadius = FMath::Clamp(FMath::Min(Extent.X, Extent.Y) * 0.36f, 650.0f, 950.0f);
    const float PatternRadius = FMath::Clamp(MinDistance * 2.4f, 560.0f, 700.0f);
    const float RandomRadius = FMath::Clamp(VisibleRadius * 0.55f, 420.0f, 650.0f);

    TArray<FVector> NavAnchors;
    int32 AnchorNavFail = 0;
    int32 AnchorBoundsFail = 0;

    auto TryAddNavAnchor = [&](const FVector& QueryPoint, const TCHAR* Source) -> void
    {
        FNavLocation NavLocation;
        if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, AnchorProjectExtent))
        {
            ++AnchorNavFail;
            return;
        }

        if (!DropZone.Bounds.IsInsideXY(NavLocation.Location))
        {
            ++AnchorBoundsFail;
            return;
        }

        for (const FVector& ExistingAnchor : NavAnchors)
        {
            if (FVector::DistSquared2D(ExistingAnchor, NavLocation.Location) < FMath::Square(100.0f))
            {
                return;
            }
        }

        NavAnchors.Add(NavLocation.Location);

        UE_LOG(LogTemp, Warning, TEXT("[DS] Card NavIsland Anchor Island=%d Slot=%d Zone=%s Key=%s Source=%s Location=%s NavZ=%.1f"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), Source,
            *NavLocation.Location.ToCompactString(), NavLocation.Location.Z);
    };

    TryAddNavAnchor(Center, TEXT("Center"));

    const FVector2D AnchorOffsets[] =
    {
        FVector2D(Extent.X * 0.10f, 0.0f),
        FVector2D(-Extent.X * 0.10f, 0.0f),
        FVector2D(0.0f, Extent.Y * 0.10f),
        FVector2D(0.0f, -Extent.Y * 0.10f)
    };

    for (const FVector2D& Offset : AnchorOffsets)
    {
        TryAddNavAnchor(FVector(Center.X + Offset.X, Center.Y + Offset.Y, Center.Z), TEXT("Offset"));
    }

    if (NavAnchors.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Key=%s Source=%s Reason=NoNavAnchor AnchorNavFail=%d AnchorBoundsFail=%d Center=%s Extent=%s"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
            AnchorNavFail, AnchorBoundsFail, *Center.ToCompactString(), *Extent.ToCompactString());
        return false;
    }

    float ReferenceNavZ = 0.0f;
    for (const FVector& Anchor : NavAnchors)
    {
        ReferenceNavZ += Anchor.Z;
    }
    ReferenceNavZ /= static_cast<float>(NavAnchors.Num());

    int32 NavFail = 0;
    int32 BoundsFail = 0;
    int32 ZFail = 0;
    int32 NoDropFail = 0;
    int32 DistFail = 0;
    int32 OverlapFail = 0;
    int32 OverheadFail = 0;
    int32 TotalCandidates = 0;

    auto TryAcceptNavLocation = [&](const FNavLocation& NavLocation, const TCHAR* Source, int32 Attempt) -> bool
    {
        ++TotalCandidates;

        if (!DropZone.Bounds.IsInsideXY(NavLocation.Location))
        {
            ++BoundsFail;
            return false;
        }

        if (FVector::DistSquared2D(NavLocation.Location, Center) > FMath::Square(VisibleRadius))
        {
            ++BoundsFail;
            return false;
        }

        const FVector Candidate = NavLocation.Location + FVector(0.0f, 0.0f, CardIslandGroundOffsetZ);

        if (!IsCardDropZSane(DropZone, ReferenceNavZ, Candidate))
        {
            ++ZFail;
            return false;
        }

        if (IsInsideNoDropZone(Candidate))
        {
            ++NoDropFail;
            return false;
        }

        if (!IsFarEnoughFromIslandCards(Candidate, ExistingIslandLocations))
        {
            ++DistFail;
            return false;
        }

        if (!IsCardDropLocationClear(Candidate))
        {
            ++OverlapFail;
            return false;
        }

        if (!HasOverheadClearance(Candidate))
        {
            ++OverheadFail;
            return false;
        }

        OutLocation = Candidate;
        UE_LOG(LogTemp, Warning,
            TEXT("[DS] Card DropInstance Island=%d Slot=%d Zone=%s Key=%s ZoneSource=%s PickSource=%s Attempts=%d TotalCandidates=%d ExistingCards=%d Location=%s SpawnZ=%.1f NavZ=%.1f RefNavZ=%.1f"),
            IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
            Source, Attempt, TotalCandidates, ExistingIslandLocations.Num(), *Candidate.ToCompactString(),
            Candidate.Z, NavLocation.Location.Z, ReferenceNavZ);
        return true;
    };

    const float BaseAngleDegrees = 90.0f + static_cast<float>(IslandIndex) * 18.0f;
    const float CandidateRadii[] = { PatternRadius, PatternRadius * 0.72f, PatternRadius * 1.18f };
    int32 PatternAttempt = 0;

    for (float CandidateRadius : CandidateRadii)
    {
        for (int32 Step = 0; Step < 5; ++Step)
        {
            ++PatternAttempt;

            const int32 PatternIndex = (SlotIndex + Step) % 5;
            const float AngleDegrees = BaseAngleDegrees + static_cast<float>(PatternIndex) * 72.0f;
            const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
            const FVector QueryPoint(
                Center.X + FMath::Cos(AngleRadians) * CandidateRadius,
                Center.Y + FMath::Sin(AngleRadians) * CandidateRadius,
                Center.Z);

            FNavLocation NavLocation;
            if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, AnchorProjectExtent))
            {
                ++NavFail;
                continue;
            }

            if (TryAcceptNavLocation(NavLocation, TEXT("NavPattern"), PatternAttempt))
            {
                return true;
            }
        }
    }

    for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
    {
        const FVector& Anchor = NavAnchors[(Attempt + SlotIndex + IslandIndex) % NavAnchors.Num()];

        FNavLocation NavLocation;
        if (!NavSystem->GetRandomReachablePointInRadius(Anchor, RandomRadius, NavLocation))
        {
            ++NavFail;
            continue;
        }

        if (TryAcceptNavLocation(NavLocation, TEXT("NavRandom"), Attempt))
        {
            return true;
        }
    }

    const int32 SpiralRings = 10;
    const int32 PointsPerRing = 16;
    const float GoldenAngleDegrees = 137.50777f;
    int32 SpiralAttempt = 0;

    for (int32 Ring = 1; Ring <= SpiralRings; ++Ring)
    {
        const float RingAlpha = static_cast<float>(Ring) / static_cast<float>(SpiralRings);
        const float RingRadius = FMath::Lerp(MinDistance, RandomRadius, RingAlpha);

        for (int32 PointIndex = 0; PointIndex < PointsPerRing; ++PointIndex)
        {
            ++SpiralAttempt;

            const FVector& Anchor = NavAnchors[(PointIndex + SlotIndex + IslandIndex) % NavAnchors.Num()];
            const float AngleDegrees = GoldenAngleDegrees * static_cast<float>(PointIndex + SlotIndex * 3 + IslandIndex * 7)
                + 360.0f * RingAlpha;
            const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

            const FVector QueryPoint(
                Anchor.X + FMath::Cos(AngleRadians) * RingRadius,
                Anchor.Y + FMath::Sin(AngleRadians) * RingRadius,
                Anchor.Z);

            FNavLocation NavLocation;
            if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, CandidateProjectExtent))
            {
                ++NavFail;
                continue;
            }

            if (TryAcceptNavLocation(NavLocation, TEXT("NavSpiral"), SpiralAttempt))
            {
                return true;
            }
        }
    }

    UE_LOG(LogTemp, Error,
        TEXT("[DS] Card DropFail Island=%d Slot=%d Zone=%s Key=%s ZoneSource=%s Reason=AllNavCandidatesRejected Anchors=%d MaxAttempts=%d SpiralCandidates=%d VisibleRadius=%.0f RandomRadius=%.0f RefNavZ=%.1f NavFail=%d BoundsFail=%d ZFail=%d DistFail=%d OverlapFail=%d OverheadFail=%d NoDrop=%d ExistingCards=%d TotalCandidates=%d"),
        IslandIndex, SlotIndex, *GetNameSafe(DropZone.ZoneActor.Get()), *DropZone.IslandKey.ToString(), *DropZone.Source,
        NavAnchors.Num(), MaxAttempts, SpiralRings * PointsPerRing, VisibleRadius, RandomRadius, ReferenceNavZ,
        NavFail, BoundsFail, ZFail, DistFail, OverlapFail, OverheadFail, NoDropFail, ExistingIslandLocations.Num(), TotalCandidates);
    return false;
}

bool FCardPlacementService::PickDeathCardDropLocation(const FVector& DeathLocation, const TArray<FVector>& ExistingDropLocations, int32 CardIndex, FVector& OutLocation) const
{
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DeathDropLocationFail CardIndex=%d Reason=NoWorld Death=%s"),
            CardIndex,
            *DeathLocation.ToCompactString());
        return false;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] Card DeathDropLocationFail CardIndex=%d Reason=NoNavSystem Death=%s"),
            CardIndex,
            *DeathLocation.ToCompactString());
        return false;
    }

    const int32 MaxAttempts = FMath::Max(8, CardDeathDropMaxAttemptsPerCard);
    const float StartRadius = FMath::Max(0.0f, CardDeathDropStartRadius);
    const float RadiusStep = FMath::Max(25.0f, CardDeathDropRadiusStep);
    const float MaxRadius = FMath::Max(StartRadius, CardDeathDropMaxRadius);
    const float MaxProjectDistance = FMath::Max(50.0f, CardDeathDropMaxNavProjectDistance);
    const float MinDistance = FMath::Max(0.0f, CardDeathDropMinCardDistance);
    const float GroundOffsetZ = FMath::Max(0.0f, CardDeathDropGroundOffsetZ);
    const FVector ProjectExtent(
        FMath::Max(80.0f, CardDeathDropNavProjectExtent.X),
        FMath::Max(80.0f, CardDeathDropNavProjectExtent.Y),
        FMath::Max(700.0f, CardDeathDropNavProjectExtent.Z));

    int32 NavFail = 0;
    int32 ProjectDistanceFail = 0;
    int32 DeathDistanceFail = 0;
    int32 NoDropFail = 0;
    int32 DistFail = 0;
    int32 OverlapFail = 0;
    int32 OverheadFail = 0;
    int32 TotalCandidates = 0;

    auto IsFarEnoughFromDeathCards = [&](const FVector& Candidate) -> bool
    {
        if (MinDistance <= 0.0f)
        {
            return true;
        }

        const float MinDistanceSq = FMath::Square(MinDistance);
        for (const FVector& ExistingLocation : ExistingDropLocations)
        {
            if (FVector::DistSquared2D(Candidate, ExistingLocation) < MinDistanceSq)
            {
                return false;
            }
        }

        return true;
    };

    auto TryAcceptQueryPoint = [&](const FVector& QueryPoint, const TCHAR* Source, int32 Attempt) -> bool
    {
        ++TotalCandidates;

        FNavLocation NavLocation;
        if (!NavSystem->ProjectPointToNavigation(QueryPoint, NavLocation, ProjectExtent))
        {
            ++NavFail;
            return false;
        }

        if (FVector::Dist2D(QueryPoint, NavLocation.Location) > MaxProjectDistance)
        {
            ++ProjectDistanceFail;
            return false;
        }

        if (FVector::Dist2D(DeathLocation, NavLocation.Location) > MaxRadius)
        {
            ++DeathDistanceFail;
            return false;
        }

        const FVector Candidate = NavLocation.Location + FVector(0.0f, 0.0f, GroundOffsetZ);

        if (IsInsideNoDropZone(Candidate))
        {
            ++NoDropFail;
            return false;
        }

        if (!IsFarEnoughFromDeathCards(Candidate))
        {
            ++DistFail;
            return false;
        }

        if (!IsCardDropLocationClear(Candidate))
        {
            ++OverlapFail;
            return false;
        }

        if (!HasOverheadClearance(Candidate))
        {
            ++OverheadFail;
            return false;
        }

        OutLocation = Candidate;

        UE_LOG(LogTemp, Warning,
            TEXT("[DS] Card DeathDropLocation CardIndex=%d Source=%s Attempt=%d TotalCandidates=%d ExistingCards=%d Death=%s Query=%s Nav=%s Spawn=%s ProjectDist=%.1f DeathDist=%.1f"),
            CardIndex,
            Source,
            Attempt,
            TotalCandidates,
            ExistingDropLocations.Num(),
            *DeathLocation.ToCompactString(),
            *QueryPoint.ToCompactString(),
            *NavLocation.Location.ToCompactString(),
            *Candidate.ToCompactString(),
            FVector::Dist2D(QueryPoint, NavLocation.Location),
            FVector::Dist2D(DeathLocation, NavLocation.Location));

        return true;
    };

    const int32 PointsPerRing = 8;
    const float BaseAngleDegrees = 90.0f + static_cast<float>(CardIndex) * 72.0f;

    for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
    {
        const int32 ZeroBasedAttempt = Attempt - 1;
        const int32 RingIndex = ZeroBasedAttempt / PointsPerRing;
        const int32 PointIndex = ZeroBasedAttempt % PointsPerRing;
        const float Radius = FMath::Min(MaxRadius, StartRadius + static_cast<float>(RingIndex) * RadiusStep);
        const float AngleDegrees = BaseAngleDegrees + static_cast<float>(PointIndex) * (360.0f / static_cast<float>(PointsPerRing)) + static_cast<float>(RingIndex) * 22.5f;
        const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

        const FVector QueryPoint(
            DeathLocation.X + FMath::Cos(AngleRadians) * Radius,
            DeathLocation.Y + FMath::Sin(AngleRadians) * Radius,
            DeathLocation.Z);

        if (TryAcceptQueryPoint(QueryPoint, TEXT("DeathNavSpiral"), Attempt))
        {
            return true;
        }
    }

    UE_LOG(LogTemp, Error,
        TEXT("[DS] Card DeathDropLocationFail CardIndex=%d Reason=AllCandidatesRejected Death=%s MaxAttempts=%d MaxRadius=%.1f ExistingCards=%d NavFail=%d ProjectDistFail=%d DeathDistFail=%d DistFail=%d OverlapFail=%d OverheadFail=%d NoDrop=%d TotalCandidates=%d"),
        CardIndex,
        *DeathLocation.ToCompactString(),
        MaxAttempts,
        MaxRadius,
        ExistingDropLocations.Num(),
        NavFail,
        ProjectDistanceFail,
        DeathDistanceFail,
        DistFail,
        OverlapFail,
        OverheadFail,
        NoDropFail,
        TotalCandidates);

    return false;
}

