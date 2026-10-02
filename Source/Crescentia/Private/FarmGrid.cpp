#include "FarmGrid.h"
#include "FarmLog.h"

void UFarmGrid::ResetWorld()
{
	Tiles.Init(FTileData(), Width * Height);
	Trees.Reset();
	WaterLevelZ = 0.f;
}

bool UFarmGrid::IsValidCoordinate(int32 X, int32 Y) const
{
	return X >= 0 && X < Width && Y >= 0 && Y < Height;
}

int32 UFarmGrid::GetIndex(int32 X, int32 Y) const
{
	return Y * Width + X;
}

FIntPoint UFarmGrid::GetCoord(int32 Index) const
{
	return FIntPoint(Index % Width, Index / Width);
}

FTileData& UFarmGrid::GetTile(int32 X, int32 Y)
{
	check(IsValidCoordinate(X, Y));
	return Tiles[GetIndex(X, Y)];
}

const FTileData& UFarmGrid::GetTile(int32 X, int32 Y) const
{
	check(IsValidCoordinate(X, Y));
	return Tiles[GetIndex(X, Y)];
}

void UFarmGrid::LogStats(int32 Seed) const
{
	int32 Counts[static_cast<uint8>(ETileType::MAX)] = {};
	for (const FTileData& Tile : Tiles)
	{
		Counts[static_cast<uint8>(Tile.Type)]++;
	}

	const float Total = FMath::Max(1, Tiles.Num());
	UE_LOG(LogFarm, Log, TEXT("FarmGrid [Seed %d]: Water %.1f%% | Sand %.1f%% | Grass %.1f%% | Highland %.1f%%"),
		Seed,
		100.f * Counts[static_cast<uint8>(ETileType::Water)]     / Total,
		100.f * Counts[static_cast<uint8>(ETileType::Sand)]      / Total,
		100.f * Counts[static_cast<uint8>(ETileType::Grassland)] / Total,
		100.f * Counts[static_cast<uint8>(ETileType::Highland)]  / Total);
}