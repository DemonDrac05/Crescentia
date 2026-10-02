#include "FarmMap.h"
#include "FarmLog.h"
#include "FarmGenerator.h"

namespace
{
	// Measures the mesh's real bounds instead of assuming "cube 100cm, pivot at center"
	// so any mesh (pivot at bottom or center, big or small) is placed correctly.
	struct FMeshFit
	{
		FVector Size   = FVector(100.f);	// bounds size (cm)
		FVector Center = FVector::ZeroVector;	// bounds center, relative to pivot
		float   Bottom = -50.f;					// Z of the lowest point, relative to pivot
	};

	FMeshFit MeasureMesh(const UStaticMesh* Mesh)
	{
		FMeshFit Fit;
		if (Mesh)
		{
			const FBox Box = Mesh->GetBoundingBox();
			
			// Never 0: we divide by Size later (a plane has Size.Z = 0)
			Fit.Size   = Box.GetSize().ComponentMax(FVector(KINDA_SMALL_NUMBER));	
			Fit.Center = Box.GetCenter();
			Fit.Bottom = Box.Min.Z;
		}
		return Fit;
	}
}

// ----- Construction -----

AFarmMap::AFarmMap()
{
	// Must exist before any SetupAttachment, otherwise components attach to nothing.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Grid = CreateDefaultSubobject<UFarmGrid>(TEXT("Grid"));

	// Subobject names are persistence keys: never rename them after placing the actor. 
	TreeHISM     = CreateHISM(TEXT("TreeHISM"));
	WaterHISM    = CreateHISM(TEXT("WaterHISM"));
	SandHISM     = CreateHISM(TEXT("SandHISM"));
	GrassHISM    = CreateHISM(TEXT("GrassHISM"));
	HighlandHISM = CreateHISM(TEXT("HighlandHISM"));

	// Per-instance collision on 65k tiles costs too much memory; terrain collision will come from one terrain mesh
	for (auto* H : { WaterHISM.Get(), SandHISM.Get(), GrassHISM.Get(), HighlandHISM.Get() })
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	WaterPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterPlane"));
	WaterPlane->SetupAttachment(RootComponent);
	WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

#if WITH_EDITOR
	// Dragging the actor in the viewport must not regenerate the whole map every frame.
	bRunConstructionScriptOnDrag = false;
#endif
}

// ----- World lifecycle -----

void AFarmMap::Regenerate()
{
	GenerateWorldData();
	BuildVisuals();
}

// ----- Gameplay edits -----

bool AFarmMap::RemoveTree(int32 X, int32 Y)
{
	if (!Grid || !Grid->IsValidCoordinate(X, Y)) return false;

	const int32 TileIndex = Grid->GetIndex(X, Y);
	
	// 1) Data: the truth decides whether a tree exists, not the view.
	if (Grid->Trees.Remove(TileIndex) == 0) return false;
	Grid->GetTile(X, Y).bIsOccupied = false;
	
	// 2) View: may have no instance (e.g. TreeMesh not assigned). The data is still correct.
	const int32* InstancePtr = TileToTreeInstance.Find(TileIndex);
	if (!InstancePtr || !TreeHISM) return true;
	
	// Copy now: Remove() below deletes this entry, so this pointer would dangle
	const int32 Removed = *InstancePtr;
	const int32 Last    = TreeInstanceToTile.Num() - 1;
	
	FTransform LastTransform;
	TreeHISM->GetInstanceTransform(Last, LastTransform);
	
	// HISM removes with swap: the last instance is moved into the freed slot.
	TreeHISM->RemoveInstance(Removed);
	
	if (Removed != Last)
	{
		// Verify the engine behaves as assumed instead of trusting it blindly.
		FTransform NowAtRemoved;
		TreeHISM->GetInstanceTransform(Removed, NowAtRemoved);
		ensureMsgf(NowAtRemoved.Equals(LastTransform), TEXT("HISM did not remove-at-swap, RemoveTree needs review"));
		
		// Keep both directions in sync: the moved instance now lives at slot Removed.
		const int32 MovedTile = TreeInstanceToTile[Last];
		TreeInstanceToTile[Removed] = MovedTile;
		TileToTreeInstance[MovedTile] = Removed;
	}
	
	TreeInstanceToTile.Pop();
	TileToTreeInstance.Remove(TileIndex);
	
	ensure(TreeHISM->GetInstanceCount() == TreeInstanceToTile.Num());
	return true;
}

// ----- Coordinates -----

FVector AFarmMap::TileToWorld(int32 X, int32 Y) const
{
	return GetActorTransform().TransformPosition(TileToLocal(X, Y));
}

FIntPoint AFarmMap::WorldToTile(const FVector& WorldLocation) const
{
	// Inverse transform: stays correct even if the actor is rotated or scaled.
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return FIntPoint(FMath::FloorToInt(Local.X / TileSize), FMath::FloorToInt(Local.Y / TileSize));
}

float AFarmMap::GetSurfaceZ(int32 X, int32 Y) const
{
	// Scale Z = 0 would flatten the cube. Goes away with terrain mesh.
	return FMath::Max(Grid->GetTile(X, Y).Height, 1.f);
}

// ----- Engine overrides -----

void AFarmMap::BeginPlay()
{
	Super::BeginPlay();
	
	// TODO(save): if a save exists and !bForceNewWorld, load into Grid instead of generating.
	Regenerate();
}

void AFarmMap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Editor preview only. In game, BeginPlay generates the map.
	const UWorld* World = GetWorld();
	if (bLivePreview && World && !World->IsGameWorld())
	{
		Regenerate();
	}
}

// ----- Phases -----

void AFarmMap::GenerateWorldData()
{
	if (!ensure(Grid)) return;
	FarmGenerator::Generate(GenSettings, *Grid);
}

void AFarmMap::BuildVisuals()
{
	if (!ensure(Grid)) return;
	
	// Required parts first, decoration last: if water fails, ground and trees are still right
	PopulateSurface();
	PopulateTree();
	PopulateWaterSurface();
}

// ----- View builders -----

void AFarmMap::PopulateSurface()
{
	// Order MUST match ETileType declaration order (Water, Grassland, Highland, Sand).
	UHierarchicalInstancedStaticMeshComponent* ByType[] = { WaterHISM, GrassHISM, HighlandHISM, SandHISM };
	static_assert(UE_ARRAY_COUNT(ByType) == static_cast<uint8>(ETileType::MAX), "ByType must cover every ETileType");

	for (auto* H : ByType)
	{
		if (!ensureMsgf(H, TEXT("Surface HISM is null - delete and re-place the actor"))) return;
		H->ClearInstances();
	}

	if (!SurfaceMesh)
	{
		UE_LOG(LogFarm, Warning, TEXT("AFarmMap: SurfaceMesh is not assigned."));
		return;
	}
	for (auto* H : ByType) { H->SetStaticMesh(SurfaceMesh); }

	const FMeshFit Fit = MeasureMesh(SurfaceMesh);
	const bool bFlatMesh = Fit.Size.Z < 1.f;			// a plane cannot be stretched along Z

	// Batch per type, then add once: far cheaper than 65k AddInstance calls.
	TArray<FTransform> Batches[UE_ARRAY_COUNT(ByType)];

	for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
	{
		for (int32 X = 0; X < UFarmGrid::Width; X++)
		{
			const float SurfaceZ = GetSurfaceZ(X, Y);
			const FVector Scale(TileSize / Fit.Size.X, TileSize / Fit.Size.Y, bFlatMesh ? 1.f : SurfaceZ / Fit.Size.Z);

			// XY: mesh center on the tile center. Z: mesh bottom at 0, mesh top at SurfaceZ.
			FVector Location = TileToLocal(X, Y) - Fit.Center * Scale;
			Location.Z = bFlatMesh ? SurfaceZ - Fit.Bottom : -Fit.Bottom * Scale.Z;

			const uint8 TypeIndex = static_cast<uint8>(Grid->GetTile(X, Y).Type);
			if (!ensure(TypeIndex < UE_ARRAY_COUNT(ByType))) continue;

			Batches[TypeIndex].Add(FTransform(FRotator::ZeroRotator, Location, Scale));
		}
	}

	for (int32 i = 0; i < UE_ARRAY_COUNT(ByType); i++)
	{
		ByType[i]->AddInstances(Batches[i], /*bShouldReturnIndices*/ false);
	}
}

void AFarmMap::PopulateTree()
{
	if (!ensureMsgf(TreeHISM, TEXT("TreeHISM is null - delete and re-place the actor"))) return;

	TreeHISM->ClearInstances();
	TileToTreeInstance.Reset();
	TreeInstanceToTile.Reset();

	if (!TreeMesh)
	{
		UE_LOG(LogFarm, Warning, TEXT("AFarmMap: TreeMesh is not assigned."));
		return;
	}
	TreeHISM->SetStaticMesh(TreeMesh);

	const FMeshFit Fit = MeasureMesh(TreeMesh);
	
	TArray<FTransform> Transforms;
	TArray<int32> TileIndices; 
	Transforms.Reserve(Grid->Trees.Num());
	TileIndices.Reserve(Grid->Trees.Num());

	for (const TPair<int32, FTreeData>& Pair: Grid->Trees)
	{
		const int32 TileIndex = Pair.Key;
		const FTreeData& Tree = Pair.Value;
		const FIntPoint Coord = Grid->GetCoord(TileIndex);
		
		// Offset is a tile fraction; only the view knows TileSize in cm
		FVector Location = TileToLocal(Coord.X, Coord.Y) 
							+ FVector(Tree.Offset.X * TileSize, Tree.Offset.Y * TileSize, 0.f);
		
		// Tree base (bounds bottom) sits on the ground wherever the mesh pivot is
		Location.Z = GetSurfaceZ(Coord.X, Coord.Y) - Fit.Bottom * Tree.Scale;
		
		Transforms.Add(FTransform(FRotator(0.f, Tree.Yaw, 0.f), 
						  Location, 
						  FVector(Tree.Scale)));
		TileIndices.Add(TileIndex);
	}

	const TArray<int32> Indices = TreeHISM->AddInstances(Transforms, /*bShouldReturnIndices*/ true);
	if (!ensureMsgf(Indices.Num() == TileIndices.Num(), TEXT("Tree instance count mismatch"))) return;

	// Fill BOTH directions of the mapping, they must always mirror each other
	TreeInstanceToTile.SetNumUninitialized(Indices.Num());
	for (int32 i = 0; i < Indices.Num(); i++)
	{
		const int32 Instance = Indices[i];
		if (!ensure(TreeInstanceToTile.IsValidIndex(Instance))) continue;

		TreeInstanceToTile[Instance] = TileIndices[i];
		TileToTreeInstance.Add(TileIndices[i], Instance);
	}
}


void AFarmMap::PopulateWaterSurface()
{
	if (!ensureMsgf(WaterPlane, TEXT("WaterPlane is null - delete and re-place the actor"))) return;

	if (!PlaneMesh)
	{
		UE_LOG(LogFarm, Warning, TEXT("AFarmMap: PlaneMesh is not assigned, hiding water."));
		WaterPlane->SetVisibility(false);
		return;
	}
	WaterPlane->SetStaticMesh(PlaneMesh);
	WaterPlane->SetVisibility(true);

	const FMeshFit Fit = MeasureMesh(PlaneMesh);
	const FVector MapSize(UFarmGrid::Width * TileSize, UFarmGrid::Height * TileSize, 0.f);
	const FVector Scale(MapSize.X / Fit.Size.X, MapSize.Y / Fit.Size.Y, 1.f);

	FVector Location = MapSize * 0.5f - Fit.Center * Scale;
	Location.Z = Grid->WaterLevelZ;

	WaterPlane->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Location, Scale));
}

// ----- Helpers -----

FVector AFarmMap::TileToLocal(int32 X, int32 Y) const
{
	return FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.f);
}

UHierarchicalInstancedStaticMeshComponent* AFarmMap::CreateHISM(FName Name)
{
	auto* HISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
	HISM->SetupAttachment(RootComponent);
	return HISM;
}