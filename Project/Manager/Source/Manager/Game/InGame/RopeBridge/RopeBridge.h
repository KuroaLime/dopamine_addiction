// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RopeBridge.generated.h"

/**
 * Procedural rope bridge between two independently floating islands.
 *
 * START = this actor's origin, END = EndTarget actor (both free level actors). Assign StartIsland /
 * EndIsland so each end follows its island as it floats -- no parenting needed (works with Packed
 * Level Actors), and it tracks island motion in the editor viewport too (ShouldTickIfViewportsOnly).
 *
 * Structure: four upright pillars (Biseok) stand at the corners with their base on the placed points.
 * Main FOOT ropes + planks run at that placed (walkway) level. HANDRAIL ropes tie to the pillars'
 * RopeAnchor socket (the ring, up high). Thin vertical ropes connect handrail down to each plank.
 */
UCLASS()
class MANAGER_API ARopeBridge : public AActor
{
	GENERATED_BODY()

public:
	ARopeBridge();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	virtual void Tick(float DeltaTime) override;
	// Tick in the editor viewport too, so the bridge follows the islands live as they are moved.
	virtual bool ShouldTickIfViewportsOnly() const override;

	// --- Anchors ---

	// The bridge spans from a START anchor on StartIsland to an END anchor on EndIsland. Each anchor is
	// a SceneComponent added inside that island's (Packed Level) Blueprint, tagged with the matching
	// AnchorTag. The bridge finds it by tag and reads its LIVE world transform every frame, so each end
	// stays glued to its island (no parenting). Place the anchor where the ropes should tie.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Anchors")
	AActor* StartIsland;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Anchors")
	FName StartAnchorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Anchors")
	AActor* EndIsland;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Anchors")
	FName EndAnchorTag;

	// --- Exposed tuning knobs ---

	// Left/right distance between the pillars (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float BridgeWidth;

	// Front/back distance between planks (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float PlankSpacing;

	// Scales the plank mesh's width (its local Y, left<->right across the bridge).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float PlankWidthScale;

	// Scales the plank mesh's depth (its local X, along the walking direction). Bigger = chunkier boards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float PlankDepthScale;

	// How far the middle of the main (foot) ropes sags downward (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float RopeSagAmount;

	// Separate sag for the handrail ropes (cm) -- usually larger so the handrail droops more.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float HandrailSagAmount;

	// Give ONLY the handrail ropes collision (blocks the player); foot/vertical ropes stay walkthrough.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	bool bHandrailCollision;

	// Uniform scale of the four corner pillars (Biseok). 1.0 = mesh default size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float BiseokScale;

	// Thickness multiplier for the thin vertical connecting ropes (1.0 = same as main rope).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float VerticalRopeScale;

	// --- Secondary shape ---

	// How far the planks sit below the main foot ropes (cm, negative = below).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float PlankVerticalOffset;

	// Native length of one rope mesh segment (cm). SM_Rope_Segment is 1m = 100.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Shape")
	float RopeSegmentLength;

	// --- Meshes ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Meshes")
	UStaticMesh* PlankMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Meshes")
	UStaticMesh* RopeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Meshes")
	UStaticMesh* VerticalRopeMesh;

	// Pillar mesh placed at the four corners (must have the RopeAnchor socket).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Meshes")
	UStaticMesh* BiseokMesh;

	// --- Runtime update (for independently floating islands) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Update")
	float RebuildThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope Bridge|Update")
	float MinRebuildInterval;

private:
	UPROPERTY()
	USceneComponent* SceneRoot;

	UPROPERTY()
	USplineComponent* MainRopeSplineL;

	UPROPERTY()
	USplineComponent* MainRopeSplineR;

	UPROPERTY()
	USplineComponent* HandrailSplineL;

	UPROPERTY()
	USplineComponent* HandrailSplineR;

	UPROPERTY()
	UInstancedStaticMeshComponent* InstancedPlanks;

	UPROPERTY()
	UInstancedStaticMeshComponent* InstancedVerticalRopes;

	// Four upright corner pillars.
	UPROPERTY()
	UStaticMeshComponent* StartBiseokMeshL;

	UPROPERTY()
	UStaticMeshComponent* StartBiseokMeshR;

	UPROPERTY()
	UStaticMeshComponent* EndBiseokMeshL;

	UPROPERTY()
	UStaticMeshComponent* EndBiseokMeshR;

	// Pool of rope segment meshes, reused across rebuilds (no destroy/recreate -> no jitter).
	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshComponents;
	int32 SplineMeshCursor;

	FVector LastBuiltStartLocal;
	FVector LastBuiltEndLocal;
	float TimeSinceLastBuild;

	// Finds a tagged SceneComponent inside Island and returns its live world location.
	bool FindAnchorWorld(AActor* Island, FName Tag, FVector& OutWorld) const;
	FVector GetStartWorld() const;
	bool GetEndWorld(FVector& OutEndWorld) const;
	// Resolves both bridge ends into this actor's local space. Returns false if there is no valid end.
	bool GetEndpointsLocal(FVector& OutStartLocal, FVector& OutEndLocal) const;
	void RebuildBridge();

	// Places a pillar upright (yaw only) with its base (mesh origin) on AnchorLocal.
	void PlaceUprightPillar(UStaticMeshComponent* Pillar, const FVector& AnchorLocal, const FRotator& YawRot);

	void UpdateSplinePoints(USplineComponent* Spline, const FVector& Start, const FVector& Mid, const FVector& End);
	void BuildSplineMeshes(USplineComponent* Spline, bool bEnableCollision);
	FTransform MakeVerticalRopeTransform(const FVector& Top, const FVector& Bot) const;

	// Spline-mesh pool + instanced-mesh reuse helpers (smooth, cheap per-frame updates).
	USplineMeshComponent* AcquireSplineMesh();
	void TrimSplineMeshPool(int32 KeepCount);
	void ApplyInstances(UInstancedStaticMeshComponent* ISM, UStaticMesh* Mesh, const TArray<FTransform>& Xforms);
	void ClearGeneratedGeometry();
};
