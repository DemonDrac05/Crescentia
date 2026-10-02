#pragma once

#include "CoreMinimal.h"
#include "WorldGenSettings.generated.h"

// Every parameter shapes a new world. Read only by FarmGenerator
USTRUCT(BlueprintType)
struct FWorldGenSettings
{
	GENERATED_BODY()

	// ----- Seed -----
	
	UPROPERTY(EditAnywhere, Category = "Seed")
	int32 Seed = 12345;
	
	// ----- Terrain noise -----
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Noise", meta = (ClampMin = "0.001"))
	float BaseFrequency = 0.05f;
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NumOctaves = 4;
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float Persistence = 0.5f;
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Noise", meta = (ClampMin = "1", ClampMax = "4"))
	float Lacunarity = 2.0f;

	// ----- Threshold (normalized [0,1], compared against raw noise, NOT cm) -----
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "0.8"))
	float SeaLevel = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "1"))
	float MountainLevel = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Threshold", meta = (ClampMin = "0", ClampMax = "1"))
	float SandBandWidth = 0.05f;
	
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
