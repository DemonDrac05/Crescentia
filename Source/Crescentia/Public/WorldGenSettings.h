#pragma once

#include "CoreMinimal.h"
#include "NoiseLayer.h"
#include "Curves/CurveFloat.h"
#include "WorldGenSettings.generated.h"

// Map-space sides of the grid (North = row Y = 0). Not tied to UE world axes.
UENUM()
enum class EMapSide: uint8 { North, South, East, West };

// Every parameter that shapes a new world. Read only by FarmGenerator
USTRUCT(BlueprintType)
struct FWorldGenSettings
{
	GENERATED_BODY()
	
	// Fills the curves with a sensible default shape.
	FWorldGenSettings();

	// ----- Seed -----
	
	UPROPERTY(EditAnywhere, Category = "Seed")
	int32 Seed = 12345;
	
	// ----- Layout -----
	
	// How far (in tiles) the border mountains reach into the map.
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float EdgeMountainWidth = 24.f;
	
	// How hard the border pulls high ground toward it. 0 = mountains everywhere.
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0", ClampMax = "2"))
	float EdgeMountainStrength = 0.6f;
	
	UPROPERTY(EditAnywhere, Category = "Layout")
	EMapSide ExitSide = EMapSide::East;
	
	// Position of the exit along its side. 0 = start, 1 = end.
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float ExitPosition = 0.5f;
	
	// Half the exit width in tiles (4 -> an 8-tile-wide gap through the mountains).
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ExitHalfWidth = 4.f;
	
	// Center of the starting clearing, as a fraction of the map (0..1 on each axis).
	UPROPERTY(EditAnywhere, Category = "Layout")
	FVector2f HouseCenter = FVector2f(0.3f, 0.65f);
	
	// Radius of the flat clearing around the house, in tiles.
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float HouseClearingRadius = 10.f;
	
	// Percentile that exit and house are forced to. Must sit inside the plains flat segment of HeightCurve. 
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float PlainsPercentile = 0.4f;
	
	// ----- Regions -----
	
	// Large, slow shapes: decide where plains, plateaus and mountains are.
	UPROPERTY(EditAnywhere, Category = "Terrain|Regions")
	FNoiseLayer RegionNoise;
	
	// Region percentile [0,1] (= share of the map) -> base height [0,1]
	// Flat segments become flat land: plains & plateau tops.
	UPROPERTY(EditAnywhere, Category = "Terrain|Regions")
	FRuntimeFloatCurve HeightCurve;
	
	// ----- Detail -----
	
	// Small bumps added on top of the base height.
	UPROPERTY(EditAnywhere, Category = "Terrain|Detail")
	FNoiseLayer DetailNoise;
	
	// Region percentile [0,1] -> detail strength there. Near 0 on plains keeps farmland flat.
	UPROPERTY(EditAnywhere, Category = "Terrain|Detail")
	FRuntimeFloatCurve DetailAmplitudeCurve;
	
	// ----- Threshold (normalized [0,1], compared against the shaped height, NOT cm) -----
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "0.8"))
	float SeaLevel = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "1"))
	float MountainLevel = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "1"))
	float SandBandWidth = 0.04f;
	
	// ----- Height -----
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Height", meta = (ClampMin = "1"))
	float HeightScale = 300.f;
	
	// ----- Trees -----
	
	UPROPERTY(EditAnywhere, Category = "Trees", meta = (ClampMin = "0", ClampMax = "1"))
	float TreeSpawnChance = 0.15f;

	// Fraction of tile size so it stays valid for any TileSize
	UPROPERTY(EditAnywhere, Category = "Trees", meta = (ClampMin = "0", ClampMax = "0.49"))
	float MaxTreeOffset = 0.35f;
	
	UPROPERTY(EditAnywhere, Category = "Trees", meta = (ClampMin = "0.1", ClampMax = "2"))
	FVector2f TreeScaleRange = FVector2f(0.9f, 1.2f);	// X = min, Y = max
};
