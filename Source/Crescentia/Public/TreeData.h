#pragma once

#include "CoreMinimal.h"
#include "TreeData.generated.h"

// One tree in the world, as pure data
USTRUCT()
struct FTreeData
{
	GENERATED_BODY()
	
	UPROPERTY() FVector2f Offset = FVector2f::ZeroVector;	// fraction of tile, [-0.5, 0.5]
	UPROPERTY() float Yaw		 = 0.f;						// degrees, around Z
	UPROPERTY() float Scale		 = 1.f;						// uniform; 0 will make trees invisible
};
