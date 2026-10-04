#include "FarmGenerator.h"
#include "FarmGrid.h"
#include "FarmLog.h"

namespace 
{
	// One salt per generation pass. Same seed + different salt = independent random sequences,
	// so adding a new pass (ponds, rivers...) never changes the output of existing passes.	
	namespace Salt
	{
		constexpr uint32 Region = 0x1001;
		constexpr uint32 Detail = 0x1003;
		constexpr uint32 Trees  = 0x2002;
	}
	
	// Private, reproducible RNG for one pass. Unlike FMath::FRand() (shared by the whole engine),
	// nobody else can draw from this stream, so same seed always gives the same numbers.
	FRandomStream MakeStream(int32 Seed, uint32 SaltValue)
	{
		return FRandomStream(static_cast<int32>(HashCombine(GetTypeHash(Seed), SaltValue)));
	}
	
	void ValidateLayer(FNoiseLayer& NoiseLayer)
	{
		NoiseLayer.Frequency	 = FMath::Max(NoiseLayer.Frequency, 0.001f);
		NoiseLayer.NumOctaves    = FMath::Clamp(NoiseLayer.NumOctaves,	   1,     8);	// 0 -> MaxValue = 0 -> divide by 0
		NoiseLayer.Persistence   = FMath::Clamp(NoiseLayer.Persistence,	0.0f,  1.0f);	// > 1		-> small details louder than big shapes
		NoiseLayer.Lacunarity    = FMath::Clamp(NoiseLayer.Lacunarity,	1.0f,  4.0f);	// < 1		-> later octaves get coarser, not finer
	}
	
	// Returns a safe COPY of the settings. Editor meta clamps only protect the Details panel.
	// Values from Blueprints, save files or code can still be invalid, so we guard here.
	FWorldGenSettings Validate(const FWorldGenSettings& In)
	{
		FWorldGenSettings S = In;
		
		ValidateLayer(S.RegionNoise);
		ValidateLayer(S.DetailNoise);
		
		if (S.HeightCurve.GetRichCurveConst()->GetNumKeys() == 0)
		{
			UE_LOG(LogFarm, Warning, TEXT("HeightCurve is empty, using a straight line."));
			
			const FKeyHandle KeyHandle1 = S.HeightCurve.GetRichCurve()->AddKey(0,0);
			const FKeyHandle KeyHandle2 = S.HeightCurve.GetRichCurve()->AddKey(1,1);
			
			S.HeightCurve.GetRichCurve()->SetKeyInterpMode(KeyHandle1, RCIM_Linear);
			S.HeightCurve.GetRichCurve()->SetKeyInterpMode(KeyHandle2, RCIM_Linear);
		}
		
		S.MaxTreeOffset 			= FMath::Clamp(S.MaxTreeOffset,				0.0f, 0.49f);	// >= 0.5	-> tree leaves its tile
		S.SeaLevel					= FMath::Clamp(S.SeaLevel,					0.0f,  0.8f);	// > 0.8	-> most of the map ends up underwater
		S.MountainLevel 			= FMath::Clamp(S.MountainLevel,				0.0f,  1.0f);
		
		// FRandRange expects Min <= Max
		if (S.TreeScaleRange.X > S.TreeScaleRange.Y)
		{
			S.TreeScaleRange = FVector2f(S.TreeScaleRange.Y, S.TreeScaleRange.X);
		}
		
		// Sand is tested before Highland in GenerateTerrain. If the sand band reaches MountainLevel,
		// grassland disappears and sand eats the mountain foot. Sand is the least important band,
		// so it is the one we shrink
		const float MaxSand = FMath::Max(0.f, S.MountainLevel - S.SeaLevel);
		S.SandBandWidth		= FMath::Max(S.SandBandWidth, 0.f);
		
		if (S.SandBandWidth > MaxSand)
		{
			UE_LOG(LogFarm, Warning, TEXT("SandBandWidth %.2f is too wide (max %.2f), clamped.")
				, S.SandBandWidth, MaxSand);
			S.SandBandWidth = MaxSand;
		}
		return S;
	}
	
	
	// Picks where each octave's window sits on the noise map. The seed decides position,
	// so a different seed shows a different part of the same infinite map.
	// Range is [0,256) because PerlinNoise2D repeats every 256 units.
	TArray<FVector2D> BuildOctaveOffsets(const FNoiseLayer& NoiseLayer, int32 Seed, uint32 SaltValue)
	{
		FRandomStream NoiseRNG = MakeStream(Seed, SaltValue);
		
		TArray<FVector2D> OctaveOffsets;
		OctaveOffsets.SetNum(NoiseLayer.NumOctaves);
		
		for (FVector2D& OctaveOffset : OctaveOffsets)
		{
			OctaveOffset = FVector2D(NoiseRNG.FRandRange(0.f, 256.f), 
									 NoiseRNG.FRandRange(0.f, 256.f));
		}
		return OctaveOffsets;
	}
	
	// Fractal noise (fBm): stacks octaves from big shapes to small details.
	// Returns a normalized value in [0, 1]
	float SampleFbm01(int32 X, int32 Y, const FNoiseLayer& NoiseLayer, const TArray<FVector2D>& OctaveOffsets)
	{
		float Total     = 0.f;
		float Amplitude = 1.f;
		float Frequency = NoiseLayer.Frequency;
		float MaxValue  = 0.f;				// sum of all amplitudes, used to bring Total back to [-1, 1]

		for (int32 i = 0; i < NoiseLayer.NumOctaves; i++)
		{
			// Where this tile lands on the noise map: window corner + tile position * step size.
			const FVector2D P = FVector2D(X, Y) * Frequency + OctaveOffsets[i];
			Total    += FMath::PerlinNoise2D(P) * Amplitude;
			MaxValue += Amplitude;

			// Each octave: finer details (higher frequency) with less influence (lower amplitude).
			Amplitude *= NoiseLayer.Persistence;
			Frequency *= NoiseLayer.Lacunarity;
		}

		return (Total / MaxValue) * 0.5f + 0.5f; // [-1, 1] -> [0, 1]
	}
	
	// Region noise picks the land, curves turn it into height, detail adds region-dependent bumps.
	void GenerateTerrain(const FWorldGenSettings& S, UFarmGrid& World)
	{
		const TArray<FVector2D> RegionOffsets = BuildOctaveOffsets(S.RegionNoise, S.Seed, Salt::Region);
		const TArray<FVector2D> DetailOffsets = BuildOctaveOffsets(S.DetailNoise, S.Seed, Salt::Detail);
		
		const FRichCurve* HeightCurve	 = S.HeightCurve.GetRichCurveConst();
		const FRichCurve* AmplitudeCurve = S.DetailAmplitudeCurve.GetRichCurveConst();
		
		for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
		{
			for (int32 X = 0; X < UFarmGrid::Width; X++)
			{
				const float Region = SampleFbm01(X, Y, S.RegionNoise, RegionOffsets);
				const float Base   = HeightCurve->Eval(Region);
				
				const float Bump	  = SampleFbm01(X, Y, S.DetailNoise, DetailOffsets) * 2.f - 1.f;
				const float Amplitude = AmplitudeCurve->Eval(Region);
				const float H01		  = FMath::Clamp(Base + Bump * Amplitude, 0.f, 1.f);
				
				FTileData& Tile = World.GetTile(X, Y);
				Tile.Height = H01 * S.HeightScale;
					
				if (H01 < S.SeaLevel)							Tile.Type = ETileType::Water;
				else if (H01 < S.SeaLevel + S.SandBandWidth)	Tile.Type = ETileType::Sand;
				else if (H01 > S.MountainLevel)					Tile.Type = ETileType::Highland;
				else											Tile.Type = ETileType::Grassland;
			}
		}
		
		World.WaterLevelZ = S.SeaLevel * S.HeightScale; // world fact in cm, saved with the world
	}
	
	// Places trees as pure data, keyed by tile index (one tree per tile at most).
	// Keep the order of RNG draws (FRand, Offset, Yaw, Scale) unchanged:
	// adding or reordering a draw reshuffles every tree in the world
	void GenerateTrees(const FWorldGenSettings& S, UFarmGrid& World)
	{
		FRandomStream RNG = MakeStream(S.Seed, Salt::Trees);
		
		for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
		{
			for (int32 X = 0; X < UFarmGrid::Width; X++)
			{
				FTileData& Tile = World.GetTile(X, Y);
				if (Tile.Type != ETileType::Grassland)	continue;
				if (RNG.FRand() > S.TreeSpawnChance)	continue;
				
				const FVector2f Offset(RNG.FRandRange(-S.MaxTreeOffset, S.MaxTreeOffset),
									   RNG.FRandRange(-S.MaxTreeOffset, S.MaxTreeOffset));
				const float Yaw		= RNG.FRandRange(0.f, 360.f);
				const float Scale	= RNG.FRandRange(S.TreeScaleRange.X, S.TreeScaleRange.Y);
				
				FTreeData NewTree;
				NewTree.Offset  = Offset;
				NewTree.Yaw		= Yaw;
				NewTree.Scale	= Scale;
				World.Trees.Add(World.GetIndex(X,Y), NewTree);
				
				Tile.bIsOccupied = true;
			}
		}
	}
}

void FarmGenerator::Generate(const FWorldGenSettings& InSettings, UFarmGrid& OutWorld)
{
	const FWorldGenSettings S = Validate(InSettings);
	
	OutWorld.ResetWorld();				// always start from a blank world
	GenerateTerrain(S, OutWorld);
	GenerateTrees(S, OutWorld);		// after terrain: trees need tile type
	
	UE_LOG(LogFarm, Log, TEXT("FarmGenerator: world generated from seed %d"), S.Seed);
	OutWorld.LogStats(S.Seed);
}
