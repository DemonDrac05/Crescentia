#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "FarmGrid.h"
#include "WorldGenSettings.h"

#include "FarmMap.generated.h"

// Owns the farm world: generates its data, then shows it.
UCLASS()
class CRESCENTIA_API AFarmMap : public AActor
{
	GENERATED_BODY()
public:
	AFarmMap();
	
	// ----- World lifecycle -----
	
	// Generates a brand-new world and shows it. 
	UFUNCTION(CallInEditor, Category = "Generation")
	void Regenerate();
	
	// Read-only on purpose: edits must go through this class to keep the view in sync.
	const UFarmGrid* GetGrid() const { return Grid; }

	// ----- Gameplay edits (data & view change together) -----
	
	// Returns false if the tile has no tree.
	bool RemoveTree(int32 X, int32 Y);

	// ----- Coordinates -----
	
	static constexpr float TileSize = 100.f;	// cm
	
	FVector TileToWorld(int32 X, int32 Y) const;
	FIntPoint WorldToTile(const FVector& WorldLocation) const;
	float GetSurfaceZ(int32 X, int32 Y) const;	// cm, actor local space.

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// ----- Settings (editable in Details even though private) ----- 
	
	UPROPERTY(EditAnywhere, Category = "Generation") 
	FWorldGenSettings GenSettings;
	
	// Editor only: regenerate on every property change. Turn off if tweaking feels slow.
	UPROPERTY(EditAnywhere, Category = "Generation") 
	bool bLivePreview = true;
	
	UPROPERTY(EditAnywhere, Category = "Visuals") TObjectPtr<UStaticMesh> SurfaceMesh;
	UPROPERTY(EditAnywhere, Category = "Visuals") TObjectPtr<UStaticMesh> TreeMesh;
	UPROPERTY(EditAnywhere, Category = "Visuals") TObjectPtr<UStaticMesh> PlaneMesh;
	
	// ----- World data (the truth) -----
	
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Generation")
	TObjectPtr<UFarmGrid> Grid;

	// ----- Components (the view) -----
	
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WaterHISM;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SandHISM;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassHISM;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HighlandHISM;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeHISM;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WaterPlane;
	
	// View-only state: always rebuilt from world data by BuildVisuals, never saved.
	TMap<int32, int32> TileToTreeInstance;	// tile index -> HISM instance
	TArray<int32> TreeInstanceToTile;		// HISM instance -> tile index

	// ----- Phases -----
	
	void GenerateWorldData();	// decides what the world is
	void BuildVisuals();		// shows the world; must work for generated & loaded worlds
	
	// ----- View builders -----
	
	void PopulateSurface();
	void PopulateTree();
	void PopulateWaterSurface();
	
	// ----- Helpers -----
	
	FVector TileToLocal(int32 X, int32 Y) const;
	UHierarchicalInstancedStaticMeshComponent* CreateHISM(FName Name);
};