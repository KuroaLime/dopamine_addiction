// Fill out your copyright notice in the Description page of Project Settings.

#include "RopeBridge.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMeshSocket.h"

ARopeBridge::ARopeBridge()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	auto MakeSpline = [this](const TCHAR* Name)
	{
		USplineComponent* S = CreateDefaultSubobject<USplineComponent>(Name);
		S->SetupAttachment(RootComponent);
		return S;
	};
	MainRopeSplineL = MakeSpline(TEXT("MainRopeSplineL"));
	MainRopeSplineR = MakeSpline(TEXT("MainRopeSplineR"));
	HandrailSplineL = MakeSpline(TEXT("HandrailSplineL"));
	HandrailSplineR = MakeSpline(TEXT("HandrailSplineR"));

	// Planks are the walkable surface -> solid collision so the player can cross.
	InstancedPlanks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedPlanks"));
	InstancedPlanks->SetupAttachment(RootComponent);
	InstancedPlanks->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InstancedPlanks->SetCollisionObjectType(ECC_WorldStatic);
	InstancedPlanks->SetCollisionResponseToAllChannels(ECR_Block);

	// Thin vertical connecting ropes are purely decorative.
	InstancedVerticalRopes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedVerticalRopes"));
	InstancedVerticalRopes->SetupAttachment(RootComponent);
	InstancedVerticalRopes->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Four upright corner pillars.
	auto MakePillar = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		P->SetupAttachment(RootComponent);
		P->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		return P;
	};
	StartBiseokMeshL = MakePillar(TEXT("StartBiseokMeshL"));
	StartBiseokMeshR = MakePillar(TEXT("StartBiseokMeshR"));
	EndBiseokMeshL = MakePillar(TEXT("EndBiseokMeshL"));
	EndBiseokMeshR = MakePillar(TEXT("EndBiseokMeshR"));

	// Defaults
	StartAnchorTag = TEXT("BridgeAnchor");
	EndAnchorTag = TEXT("BridgeAnchor");
	BridgeWidth = 600.0f;
	PlankSpacing = 90.0f;
	PlankWidthScale = 10.0f;
	PlankDepthScale = 5.0f;
	RopeSagAmount = 80.0f;
	HandrailSagAmount = 300.0f;
	bHandrailCollision = true;
	BiseokScale = 1.0f;
	VerticalRopeScale = 0.3f;      // thin
	PlankVerticalOffset = 3.0f;
	RopeSegmentLength = 100.0f;
	RebuildThreshold = 0.1f;       // update nearly every frame while moving (cheap thanks to reuse)
	MinRebuildInterval = 0.0f;

	SplineMeshCursor = 0;
	LastBuiltStartLocal = FVector::ZeroVector;
	LastBuiltEndLocal = FVector::ZeroVector;
	TimeSinceLastBuild = 0.0f;

	// Default meshes from assets already in the project.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RopeMeshAsset(TEXT("/Game/P_MyMap/Rope/SM_Rope_Segment.SM_Rope_Segment"));
	if (RopeMeshAsset.Succeeded())
	{
		RopeMesh = RopeMeshAsset.Object;
		VerticalRopeMesh = RopeMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BiseokMeshAsset(TEXT("/Game/P_MyMap/Biseok/SM_BISEOK.SM_BISEOK"));
	if (BiseokMeshAsset.Succeeded())
	{
		BiseokMesh = BiseokMeshAsset.Object;
		StartBiseokMeshL->SetStaticMesh(BiseokMesh);
		StartBiseokMeshR->SetStaticMesh(BiseokMesh);
		EndBiseokMeshL->SetStaticMesh(BiseokMesh);
		EndBiseokMeshR->SetStaticMesh(BiseokMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlankMeshAsset(TEXT("/Game/P_MyMap/Plank/SM_Plank.SM_Plank"));
	if (PlankMeshAsset.Succeeded())
	{
		PlankMesh = PlankMeshAsset.Object;
	}
}

void ARopeBridge::BeginPlay()
{
	Super::BeginPlay();
	// Tick after the islands so we read their CURRENT (this-frame) transforms -> no 1-frame lag,
	// which is what made the pillars/ropes vibrate against the moving island.
	if (StartIsland)
	{
		AddTickPrerequisiteActor(StartIsland);
	}
	if (EndIsland)
	{
		AddTickPrerequisiteActor(EndIsland);
	}
	RebuildBridge();
}

void ARopeBridge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildBridge();
}

void ARopeBridge::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastBuild += DeltaTime;

	FVector StartLocal, EndLocal;
	if (!GetEndpointsLocal(StartLocal, EndLocal))
	{
		return;
	}

	// Either island can float independently, so rebuild when either end shifts. The threshold/interval
	// are tiny so it follows smoothly every frame (cheap because the geometry is reused, not recreated).
	const float Delta = FMath::Max(FVector::Dist(StartLocal, LastBuiltStartLocal), FVector::Dist(EndLocal, LastBuiltEndLocal));
	if (Delta > RebuildThreshold && TimeSinceLastBuild >= MinRebuildInterval)
	{
		RebuildBridge();
		TimeSinceLastBuild = 0.0f;
	}
}

bool ARopeBridge::ShouldTickIfViewportsOnly() const
{
	// Follow the islands live while editing in the viewport, not just during play.
	return true;
}

bool ARopeBridge::FindAnchorWorld(AActor* Island, FName Tag, FVector& OutWorld) const
{
	if (!Island || Tag.IsNone())
	{
		return false;
	}
	// The anchor is a SceneComponent added inside the island's Blueprint and given a Component Tag.
	TArray<USceneComponent*> Comps;
	Island->GetComponents<USceneComponent>(Comps);
	for (USceneComponent* C : Comps)
	{
		if (C && C->ComponentHasTag(Tag))
		{
			OutWorld = C->GetComponentLocation();
			return true;
		}
	}
	return false;
}

FVector ARopeBridge::GetStartWorld() const
{
	FVector W;
	if (FindAnchorWorld(StartIsland, StartAnchorTag, W))
	{
		return W;
	}
	return GetActorLocation();
}

bool ARopeBridge::GetEndWorld(FVector& OutEndWorld) const
{
	return FindAnchorWorld(EndIsland, EndAnchorTag, OutEndWorld);
}

bool ARopeBridge::GetEndpointsLocal(FVector& OutStartLocal, FVector& OutEndLocal) const
{
	FVector EndWorld;
	if (!GetEndWorld(EndWorld))
	{
		return false;
	}
	const FTransform& Xf = GetActorTransform();
	OutStartLocal = Xf.InverseTransformPosition(GetStartWorld());
	OutEndLocal = Xf.InverseTransformPosition(EndWorld);
	return true;
}

void ARopeBridge::PlaceUprightPillar(UStaticMeshComponent* Pillar, const FVector& AnchorLocal, const FRotator& YawRot)
{
	if (!Pillar)
	{
		return;
	}
	if (BiseokMesh)
	{
		Pillar->SetStaticMesh(BiseokMesh);
	}

	// The pillar mesh origin is at its base, so place the base directly on the anchor point. Only
	// BridgeWidth (baked into AnchorLocal) and yaw apply; roll/pitch stay zero so it stands upright.
	Pillar->SetRelativeScale3D(FVector(BiseokScale));
	Pillar->SetRelativeLocationAndRotation(AnchorLocal, YawRot);
}

void ARopeBridge::RebuildBridge()
{
	SplineMeshCursor = 0;

	FVector StartLocal, EndLocal;
	if (!GetEndpointsLocal(StartLocal, EndLocal))
	{
		ClearGeneratedGeometry();
		return;
	}
	LastBuiltStartLocal = StartLocal;
	LastBuiltEndLocal = EndLocal;

	FVector Dir = (EndLocal - StartLocal).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		ClearGeneratedGeometry();
		return;
	}
	const FVector Up = FVector::UpVector;
	FVector Right = FVector::CrossProduct(Dir, Up).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(Dir, FVector::ForwardVector).GetSafeNormal();
	}

	// Pillar BASE anchor points: only BridgeWidth offset (horizontal) -- NO height change.
	const FVector HalfWidth = Right * (BridgeWidth * 0.5f);
	const FVector BaseStartL = StartLocal - HalfWidth;
	const FVector BaseStartR = StartLocal + HalfWidth;
	const FVector BaseEndL = EndLocal - HalfWidth;
	const FVector BaseEndR = EndLocal + HalfWidth;

	// Upright pillar rotations: yaw only (flatten the bridge direction onto the ground plane).
	const FVector FlatDir = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
	const float YawDeg = (FlatDir.IsNearlyZero()) ? 0.f : FMath::RadiansToDegrees(FMath::Atan2(FlatDir.Y, FlatDir.X));
	const FRotator StartYaw(0.f, YawDeg, 0.f);          // start pillars face toward the far island
	const FRotator EndYaw(0.f, YawDeg + 180.f, 0.f);    // end pillars face back

	PlaceUprightPillar(StartBiseokMeshL, BaseStartL, StartYaw);
	PlaceUprightPillar(StartBiseokMeshR, BaseStartR, StartYaw);
	PlaceUprightPillar(EndBiseokMeshL, BaseEndL, EndYaw);
	PlaceUprightPillar(EndBiseokMeshR, BaseEndR, EndYaw);

	// The "RopeAnchor" socket marks the pillar's ring -- where the HANDRAIL ties (fixed name, not exposed).
	static const FName RopeAnchorSocket(TEXT("RopeAnchor"));
	FVector SocketRel = FVector::ZeroVector;
	if (BiseokMesh)
	{
		if (const UStaticMeshSocket* Socket = BiseokMesh->FindSocket(RopeAnchorSocket))
		{
			SocketRel = Socket->RelativeLocation;
		}
	}
	// The socket rides with the pillar's scale, so scale its offset too.
	const FVector ScaledSocket = SocketRel * BiseokScale;
	const FVector RingStartL = BaseStartL + StartYaw.RotateVector(ScaledSocket);
	const FVector RingStartR = BaseStartR + StartYaw.RotateVector(ScaledSocket);
	const FVector RingEndL = BaseEndL + EndYaw.RotateVector(ScaledSocket);
	const FVector RingEndR = BaseEndR + EndYaw.RotateVector(ScaledSocket);

	// Main FOOT ropes run at the placed (walkway) level between the pillar bases, sagging in the middle.
	const FVector FootSag(0.f, 0.f, RopeSagAmount);
	const FVector MainMidL = FMath::Lerp(BaseStartL, BaseEndL, 0.5f) - FootSag;
	const FVector MainMidR = FMath::Lerp(BaseStartR, BaseEndR, 0.5f) - FootSag;
	UpdateSplinePoints(MainRopeSplineL, BaseStartL, MainMidL, BaseEndL);
	UpdateSplinePoints(MainRopeSplineR, BaseStartR, MainMidR, BaseEndR);

	// HANDRAIL ropes tie to the pillar rings (sockets) and use their own (usually larger) sag.
	const FVector HandSag(0.f, 0.f, HandrailSagAmount);
	const FVector HandMidL = FMath::Lerp(RingStartL, RingEndL, 0.5f) - HandSag;
	const FVector HandMidR = FMath::Lerp(RingStartR, RingEndR, 0.5f) - HandSag;
	UpdateSplinePoints(HandrailSplineL, RingStartL, HandMidL, RingEndL);
	UpdateSplinePoints(HandrailSplineR, RingStartR, HandMidR, RingEndR);

	// Foot ropes never block; only the handrails optionally collide.
	BuildSplineMeshes(MainRopeSplineL, false);
	BuildSplineMeshes(MainRopeSplineR, false);
	BuildSplineMeshes(HandrailSplineL, bHandrailCollision);
	BuildSplineMeshes(HandrailSplineR, bHandrailCollision);

	// Gather plank + thin vertical-rope transforms, then apply them (reusing instances to avoid jitter).
	TArray<FTransform> PlankXforms;
	TArray<FTransform> VRopeXforms;
	if (PlankMesh)
	{
		const FVector PlankDrop(0.f, 0.f, PlankVerticalOffset);
		const float SplineLength = MainRopeSplineL->GetSplineLength();
		const int32 NumPlanks = FMath::FloorToInt(SplineLength / PlankSpacing);

		for (int32 i = 1; i < NumPlanks; ++i)
		{
			const float Dist = i * PlankSpacing;

			const FVector PosL = MainRopeSplineL->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
			const FVector PosR = MainRopeSplineR->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
			FVector Center = FMath::Lerp(PosL, PosR, 0.5f);

			// Orient the plank to follow the sag slope (X = travel, Y = across, Z = surface up).
			const FVector TanL = MainRopeSplineL->GetTangentAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local).GetSafeNormal();
			const FVector TanR = MainRopeSplineR->GetTangentAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local).GetSafeNormal();
			const FVector Travel = (TanL + TanR).GetSafeNormal();
			const FVector Across = (PosR - PosL).GetSafeNormal();

			FVector PlankUp = FVector::CrossProduct(Travel, Across).GetSafeNormal();
			if (PlankUp.Z < 0.f)
			{
				PlankUp = -PlankUp;
			}
			const FRotator Rot = FRotationMatrix::MakeFromXZ(Travel, PlankUp).Rotator();

			Center += PlankDrop;
			PlankXforms.Add(FTransform(Rot, Center, FVector(PlankDepthScale, PlankWidthScale, 1.f)));

			if (VerticalRopeMesh)
			{
				const FVector TopL = HandrailSplineL->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
				VRopeXforms.Add(MakeVerticalRopeTransform(TopL, PosL + PlankDrop));

				const FVector TopR = HandrailSplineR->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
				VRopeXforms.Add(MakeVerticalRopeTransform(TopR, PosR + PlankDrop));
			}
		}
	}

	ApplyInstances(InstancedPlanks, PlankMesh, PlankXforms);
	ApplyInstances(InstancedVerticalRopes, VerticalRopeMesh, VRopeXforms);

	// Free any rope segment meshes left unused this rebuild.
	TrimSplineMeshPool(SplineMeshCursor);
}

void ARopeBridge::UpdateSplinePoints(USplineComponent* Spline, const FVector& Start, const FVector& Mid, const FVector& End)
{
	if (!Spline)
	{
		return;
	}

	Spline->ClearSplinePoints(false);
	Spline->AddSplinePoint(Start, ESplineCoordinateSpace::Local, false);
	Spline->AddSplinePoint(Mid, ESplineCoordinateSpace::Local, false);
	Spline->AddSplinePoint(End, ESplineCoordinateSpace::Local, false);
	Spline->UpdateSpline();
}

USplineMeshComponent* ARopeBridge::AcquireSplineMesh()
{
	// Reuse a pooled component if available; otherwise create one. All segments live in actor-local
	// space (attached to root), so the same pool can serve any of the four splines.
	if (SplineMeshCursor < SplineMeshComponents.Num() && SplineMeshComponents[SplineMeshCursor])
	{
		USplineMeshComponent* Existing = SplineMeshComponents[SplineMeshCursor++];
		return Existing;
	}

	USplineMeshComponent* Mesh = NewObject<USplineMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetForwardAxis(ESplineMeshAxis::X);
	Mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Mesh->RegisterComponent();

	if (SplineMeshCursor < SplineMeshComponents.Num())
	{
		SplineMeshComponents[SplineMeshCursor] = Mesh;
	}
	else
	{
		SplineMeshComponents.Add(Mesh);
	}
	++SplineMeshCursor;
	return Mesh;
}

void ARopeBridge::TrimSplineMeshPool(int32 KeepCount)
{
	for (int32 i = SplineMeshComponents.Num() - 1; i >= KeepCount; --i)
	{
		if (SplineMeshComponents[i])
		{
			SplineMeshComponents[i]->DestroyComponent();
		}
		SplineMeshComponents.RemoveAt(i);
	}
}

void ARopeBridge::BuildSplineMeshes(USplineComponent* Spline, bool bEnableCollision)
{
	if (!Spline || !RopeMesh)
	{
		return;
	}

	const float TotalLength = Spline->GetSplineLength();
	const float SegmentLength = FMath::Max(RopeSegmentLength, 1.0f);
	const int32 NumSegments = FMath::Max(FMath::CeilToInt(TotalLength / SegmentLength), 1);

	for (int32 i = 0; i < NumSegments; ++i)
	{
		const float StartDist = i * (TotalLength / NumSegments);
		const float EndDist = (i + 1) * (TotalLength / NumSegments);

		FVector StartPos = Spline->GetLocationAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
		FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
		FVector EndPos = Spline->GetLocationAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);
		FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);

		const float SegLen = EndDist - StartDist;
		StartTangent = StartTangent.GetSafeNormal() * SegLen;
		EndTangent = EndTangent.GetSafeNormal() * SegLen;

		USplineMeshComponent* SplineMesh = AcquireSplineMesh();
		SplineMesh->SetStaticMesh(RopeMesh);
		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true); // rope thickness stays 1.0
		SplineMesh->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bEnableCollision)
		{
			SplineMesh->SetCollisionObjectType(ECC_WorldStatic);
			SplineMesh->SetCollisionResponseToAllChannels(ECR_Block);
		}
	}
}

FTransform ARopeBridge::MakeVerticalRopeTransform(const FVector& Top, const FVector& Bot) const
{
	const FVector Dir = (Bot - Top).GetSafeNormal();
	const float Length = (Bot - Top).Size();

	// X spans the gap (top->bottom). Use MakeFromX so the roll is derived consistently from the
	// direction -- MakeFromXZ with a fixed world-Y reference made each rope twist differently.
	const FRotator Rot = FRotationMatrix::MakeFromX(Dir).Rotator();
	const FVector Scale(Length / FMath::Max(RopeSegmentLength, 1.0f), VerticalRopeScale, VerticalRopeScale);
	return FTransform(Rot, Top, Scale);
}

void ARopeBridge::ApplyInstances(UInstancedStaticMeshComponent* ISM, UStaticMesh* Mesh, const TArray<FTransform>& Xforms)
{
	if (!ISM)
	{
		return;
	}
	if (Mesh && ISM->GetStaticMesh() != Mesh)
	{
		ISM->SetStaticMesh(Mesh);
	}

	if (ISM->GetInstanceCount() == Xforms.Num())
	{
		// Same count -> just move the existing instances. Keeps PerInstanceRandom stable and is cheap,
		// so it can run every frame without jitter or texture flicker.
		for (int32 i = 0; i < Xforms.Num(); ++i)
		{
			ISM->UpdateInstanceTransform(i, Xforms[i], false, false, true);
		}
		ISM->MarkRenderStateDirty();
	}
	else
	{
		ISM->ClearInstances();
		for (const FTransform& T : Xforms)
		{
			ISM->AddInstance(T, false); // local space
		}
	}
}

void ARopeBridge::ClearGeneratedGeometry()
{
	TrimSplineMeshPool(0);
	if (InstancedPlanks)
	{
		InstancedPlanks->ClearInstances();
	}
	if (InstancedVerticalRopes)
	{
		InstancedVerticalRopes->ClearInstances();
	}
}
