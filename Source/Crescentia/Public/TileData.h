#pragma once

#include "CoreMinimal.h"
#include "TileData.generated.h"

UENUM(BlueprintType)
enum class ETileType : uint8
{
	Water,
	Grassland,
	Highland,
	Sand,

	// MAX must stay last: it is used as the number of tile types
	MAX UMETA(Hidden)
};

// One cell of the world grid. Height is in cm
USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType Type = ETileType::Grassland;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Height = 0.0f;

	// Cache of "something stands here". Update it wherever Trees (later rocks, crops) change.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsOccupied = false;
};
