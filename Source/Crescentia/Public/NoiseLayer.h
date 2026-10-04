#pragma once

#include "CoreMinimal.h"
#include "NoiseLayer.generated.h"

// One fractal noise layer (fBm). Reused for every noise the generator needs.
USTRUCT(BlueprintType)
struct FNoiseLayer
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.001"))
	float Frequency = 0.05f;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "8"))
	int32 NumOctaves = 4;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "1"))
	float Persistence = 0.5f;
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "4"))
	float Lacunarity = 2.0f;
};
