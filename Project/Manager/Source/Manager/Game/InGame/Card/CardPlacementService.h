// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/Protocol_Client/Protocol_InGame.h"

class UWorld;
class AActor;
struct FHitResult;

enum class ECardDropPlacementSource : uint8
{
    None,
    Navigation,
    GroundTraceFallback
};

// 카드 섬 드롭 후보 구역. 기존 AMainGameMode 내부 private 구조체에서 이전.
struct FCardIslandDropZone
{
    TWeakObjectPtr<AActor> ZoneActor;
    FBox Bounds;
    FVector Center = FVector::ZeroVector;
    FName IslandKey = NAME_None;
    FString Source;
    int32 SortOrder = 1000;
};

/**
 * 카드 배치(번들 구성/섬 드롭존 탐색/지면·내비·간격 판정/사망 드롭) 순수 로직 서비스.
 * GameMode 상태를 갖지 않으며, 동작에 필요한 설정값과 World만 보유한다.
 * 설정 필드 이름은 AMainGameMode와 동일하게 두어 이전된 본문을 거의 그대로 사용한다.
 * AMainGameMode::MakeCardPlacementService() 로 생성/주입한다.
 */
class MANAGER_API FCardPlacementService
{
public:
    UWorld* World = nullptr;

    // --- Bundle ---
    FVector CardBundleDropCenter = FVector(0.0f, 0.0f, 180.0f);
    FVector2D CardBundleDropExtent = FVector2D(1200.0f, 800.0f);
    float CardBundleDropJitterRatio = 0.1f;

    // --- Island ---
    FName CardIslandDropZoneTag = TEXT("CardIslandDropZone");
    bool bAutoDetectSeasonIslandActorsAsDropZones = true;
    int32 CardIslandDropExpectedZoneCount = 4;
    int32 CardIslandDropMaxAttemptsPerCard = 160;
    float CardIslandGroundTraceHalfHeight = 5000.0f;
    float CardIslandGroundOffsetZ = 80.0f;
    float CardIslandMinCardDistance = 250.0f;
    bool bProjectCardDropsToNavigation = true;
    FVector CardIslandNavProjectExtent = FVector(200.0f, 200.0f, 500.0f);
    float CardIslandMaxGroundSlopeDegrees = 35.0f;
    FVector CardIslandOverlapBoxExtent = FVector(80.0f, 80.0f, 60.0f);
    FName CardNoDropZoneTag = TEXT("CardNoDropZone");
    float CardIslandMaxGroundZDelta = 400.0f;
    float CardIslandOverheadClearance = 90.0f;

    // --- Death drop ---
    int32 CardDeathDropMaxAttemptsPerCard = 32;
    float CardDeathDropStartRadius = 120.0f;
    float CardDeathDropRadiusStep = 80.0f;
    float CardDeathDropMaxRadius = 520.0f;
    float CardDeathDropMinCardDistance = 120.0f;
    float CardDeathDropGroundOffsetZ = 80.0f;
    FVector CardDeathDropNavProjectExtent = FVector(180.0f, 180.0f, 500.0f);
    float CardDeathDropMaxNavProjectDistance = 180.0f;

public:
    TArray<ECardID> BuildCardBundleIDs() const;
    void ShuffleCardIDs(TArray<ECardID>& CardIDs) const;
    FVector GetDistributedCardDropLocation(int32 Index, int32 TotalCount) const;

    int32 GetCardIslandBalanceValue(ECardID CardID) const;
    int32 GetCardIslandGroupBalanceValue(const TArray<ECardID>& CardIDs) const;
    TArray<TArray<ECardID>> BuildBalancedIslandCardGroups() const;

    bool IsSeasonIslandActorName(const FString& ActorName) const;
    TArray<FCardIslandDropZone> FindCardIslandDropZones() const;

    bool IsCardIslandSurfaceWalkable(const FHitResult& Hit) const;
    bool IsCardDropLocationClear(const FVector& CandidateLocation) const;
    bool IsFarEnoughFromIslandCards(const FVector& CandidateLocation, const TArray<FVector>& ExistingIslandLocations) const;
    bool IsCardDropZSane(const FCardIslandDropZone& DropZone, float ReferenceNavZ, const FVector& Candidate) const;
    bool IsInsideNoDropZone(const FVector& Candidate) const;
    bool HasOverheadClearance(const FVector& Candidate) const;

    // Why a candidate was turned down, so each caller can keep its own diagnostic counters.
    enum class ECardDropReject : uint8
    {
        Accepted,
        ZOutOfRange,
        InsideNoDropZone,
        TooCloseToOtherCards,
        Blocked,
        NoOverheadClearance,
    };

    // The island placement path and its ground-trace fallback ran the same five checks in the same
    // order, written out twice; the death-drop path repeats a subset twice more. Keeping the order and
    // the set of checks in one place stops the copies from drifting when a check is added or reordered.
    // Callers still choose their own control flow (return vs continue) and bump their own counters.
    ECardDropReject ClassifyIslandCandidate(
        const FCardIslandDropZone& DropZone,
        float ReferenceZ,
        const FVector& Candidate,
        const TArray<FVector>& ExistingIslandLocations) const;

    bool PickIslandCardDropLocation(
        const FCardIslandDropZone& DropZone,
        const TArray<FVector>& ExistingIslandLocations,
        int32 IslandIndex,
        int32 SlotIndex,
        FVector& OutLocation,
        ECardDropPlacementSource* OutSource = nullptr,
        FString* OutFailureReason = nullptr) const;
    bool PickDeathCardDropLocation(const FVector& DeathLocation, const TArray<FVector>& ExistingDropLocations, int32 CardIndex, FVector& OutLocation) const;
};
