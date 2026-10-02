#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TileData.h"
#include "TreeData.h"
#include "FarmGrid.generated.h"

// The world as pure data: what exists, not how it was made nor how it is drawn
// Must never include generator, render or save code (dependency rule)
UCLASS(DefaultToInstanced, EditInlineNew)
class CRESCENTIA_API UFarmGrid : public UObject
{
	GENERATED_BODY()
public:
	static constexpr int32 Width  = 256;
	static constexpr int32 Height = 256;

	UPROPERTY() TArray<FTileData> Tiles;
	UPROPERTY() TMap<int32, FTreeData> Trees;	// key = tile index, one tree per tile at most
	UPROPERTY() float WaterLevelZ = 0.f;		// cm

	// Wipes every piece of world state. New state added later must be reset here too,
	// or regenerating leaks data from the previous world
	void ResetWorld();

	bool IsValidCoordinate(int32 X, int32 Y) const;
	int32 GetIndex(int32 X, int32 Y) const;
	FIntPoint GetCoord(int32 Index) const;
	FTileData& GetTile(int32 X, int32 Y);
	const FTileData& GetTile(int32 X, int32 Y) const;

	void LogStats(int32 Seed) const;
};