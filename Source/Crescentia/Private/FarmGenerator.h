#pragma once

#include "CoreMinimal.h"
#include "WorldGenSettings.h"

class UFarmGrid;

// Builds a new world from settings. Stateless: the same settings always give the same world
namespace FarmGenerator
{
	void Generate(const FWorldGenSettings& InSettings, UFarmGrid& OutWorld);
}
