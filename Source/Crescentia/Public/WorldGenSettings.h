#pragma once

#include "CoreMinimal.h"
#include "NoiseLayer.h"
#include "Curves/CurveFloat.h"
#include "WorldGenSettings.generated.h"

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
	
	// ----- Regions -----
	
	// Large, slow shapes: decide where plains, plateaus and mountains are.
	UPROPERTY(EditAnywhere, Category = "Terrain|Regions")
	FNoiseLayer RegionNoise;
	
	// Region value [0,1] -> base height [0,1]
	// Flat segments become flat land: plains & plateau tops.
	UPROPERTY(EditAnywhere, Category = "Terrain|Regions")
	FRuntimeFloatCurve HeightCurve;
	
	// ----- Detail -----
	
	// Small bumps added on top of the base height.
	UPROPERTY(EditAnywhere, Category = "Terrain|Detail")
	FNoiseLayer DetailNoise;
	
	// Region value [0,1] -> detail strength there. Near 0 on plains keeps farmland flat.
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
