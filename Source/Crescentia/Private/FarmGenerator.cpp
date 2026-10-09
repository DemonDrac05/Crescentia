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
		
		S.PlainsPercentile	  = FMath::Clamp(S.PlainsPercentile, 0.f, 1.f);
		S.EdgeMountainWidth   = FMath::Max(S.EdgeMountainWidth, 1.f);
		S.ExitHalfWidth		  = FMath::Max(S.ExitHalfWidth, 1.f);
		S.HouseClearingRadius = FMath::Max(S.HouseClearingRadius, 1.f);
		S.HouseCenter		  = FVector2f(FMath::Clamp(S.HouseCenter.X, 0.f, 1.f), 
										  FMath::Clamp(S.HouseCenter.Y, 0.f, 1.f));
		
		if (S.HeightCurve.GetRichCurveConst()->GetNumKeys() == 0)
		{
			UE_LOG(LogFarm, Warning, TEXT("HeightCurve is empty, using a straight line."));
			
			const FKeyHandle KeyHandle1 = S.HeightCurve.GetRichCurve()->AddKey(0,0);
			const FKeyHandle KeyHandle2 = S.HeightCurve.GetRichCurve()->AddKey(1,1);
			
			S.HeightCurve.GetRichCurve()->SetKeyInterpMode(KeyHandle1, RCIM_Linear);
			S.HeightCurve.GetRichCurve()->SetKeyInterpMode(KeyHandle2, RCIM_Linear);
		}
		
		// PlainsPercentile must land on flat ground, or the house and exit end up on a slope.
		const FRichCurve* Height = S.HeightCurve.GetRichCurveConst();
		const float Before = Height->Eval(S.PlainsPercentile - 0.01f);
		const float After  = Height->Eval(S.PlainsPercentile + 0.01f);
		if (!FMath::IsNearlyEqual(Before, After, 0.01f))
		{
			UE_LOG(LogFarm, 
				   Warning, 
				   TEXT("PlainsPercentile %.2f is not a flat segment of HeightCurve."), S.PlainsPercentile);
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
	
	// Distance in tiles from the tile to the nearest map border (0 on the border itself).
	int32 DistanceToBorder(int32 X, int32 Y)
	{
		return FMath::Min(X, Y, UFarmGrid::Width - 1 - X, UFarmGrid::Height - 1 - Y);
	}
	
	// Rewrites a tile position relative to one map side:
	// X = position along that side, Y = distance inward from it.
	// Lets one piece of code handle exits on any side.
	FVector2f ToSideSpace(int32 X, int32 Y, EMapSide Side)
	{
		switch (Side)
		{
			case EMapSide::North: return FVector2f(X, Y);
			case EMapSide::South: return FVector2f(X, UFarmGrid::Height - 1 - Y);
			case EMapSide::West:  return FVector2f(Y, X);
			case EMapSide::East:  return FVector2f(Y, UFarmGrid::Width - 1 - X);
		}
		return FVector2f::ZeroVector;
	}
	
	// Phase 1: raw region noise plus soft placement biases.
	// Only decides where things lean to; shares are decided by the ranking.
	TArray<float> BuildRegionMap(const FWorldGenSettings& S)
	{
		const TArray<FVector2D> Offsets = BuildOctaveOffsets(S.RegionNoise, S.Seed, Salt::Region);
		
		TArray<float> Region;
		Region.SetNumUninitialized(UFarmGrid::Height * UFarmGrid::Width);
		
		for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
		{
			for (int32 X = 0; X < UFarmGrid::Width; X++)
			{
				float R = SampleFbm01(X, Y, S.RegionNoise, Offsets);
				
				// 1 on the border, fading to 0 at EdgeMountainWidth tiles inward:
				// high ground leans toward the edges, so mountains frame the farm.
				const float Edge = 1.f - FMath::SmoothStep(0.f, 
														   S.EdgeMountainWidth,
														   static_cast<float>(DistanceToBorder(X, Y)));
				R += Edge * S.EdgeMountainStrength;
				
				Region[Y * UFarmGrid::Width + X] = R;
			}
		}
		return Region;
	}
	
	// Phase 2: replace each value by its rank / (N -1), spreading values evenly over [0,1].
	// Afterward, X on every curve means "share of the map": a flat segment 0.6 wide = 60% of tiles.
	void RemapToPercentile(TArray<float>& Values)
	{
		const int32 N = Values.Num();
		
		TArray<int32> Order;
		Order.SetNumUninitialized(N);
		for (int32 i = 0; i < N; i++) Order[i] = i;
		
		// Ties broken by index: equal values always rank the same way, keeping generation deterministic.
		Order.Sort([&Values](const int32 A, const int32 B)
		{
			return Values[A] < Values[B] || (Values[A] == Values[B] && A < B);
		});
		
		for (int32 Rank = 0; Rank < N; Rank++)
		{
			Values[Order[Rank]] = Rank / static_cast<float>(N - 1);
		}
	}
	
	// Phase 3: guarantees that must hold for every seed. Runs after the ranking so they always win.
	// Trade-off: these few tiles shift the shares slightly; acceptable for a clearing & a gap.
	void ApplyLayoutGuarantee(const FWorldGenSettings& S, TArray<float>& Percentile)
	{
		constexpr float SoftMargin = 3.f; // tiles of blending 
		
		// Must cut deeper than the mountain band, or the gap ends inside the mountain.
		const float ExitDepth  = S.EdgeMountainWidth * 1.5f;
		const float SideLength = (S.ExitSide == EMapSide::North || S.ExitSide == EMapSide::South) 
			? UFarmGrid::Width : UFarmGrid::Height;
		const float ExitCenter = S.ExitPosition * (SideLength - 1);
		
		const FVector2f House(S.HouseCenter.X * (UFarmGrid::Width - 1), 
							  S.HouseCenter.Y * (UFarmGrid::Height - 1));
		
		for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
		{
			for (int32 X = 0; X < UFarmGrid::Width; X++)
			{
				// Exit corridor: narrow across the side, deep inward.
				const FVector2f Side = ToSideSpace(X, Y, S.ExitSide);
				const float Across	 = FMath::Abs(Side.X - ExitCenter);
				const float AcrossW  = 1.f - FMath::SmoothStep(S.ExitHalfWidth, 
															   S.ExitHalfWidth + SoftMargin, 
															   Across);
				const float InwardW  = 1.f - FMath::SmoothStep(ExitDepth,
															   ExitDepth + SoftMargin * 2.f,
															   Side.Y);
				const float ExitW	 = AcrossW * InwardW;
				
				// 1 in the inner 70% of the radius (fully flat core), fading to 0 at the radius (soft rim).
				const float Dist   = FVector2f::Distance(FVector2f(X, Y), House);
				const float HouseW = 1.f - FMath::SmoothStep(S.HouseClearingRadius * 0.7f, S.HouseClearingRadius, Dist);
				
				const int32 Index = Y * UFarmGrid::Width + X;
				Percentile[Index] = FMath::Lerp(Percentile[Index], 
												S.PlainsPercentile, 
												FMath::Max(ExitW, HouseW));
			}
		}
	}
	
		
	
	// Region noise picks the land, curves turn it into height, detail adds region-dependent bumps.
	void GenerateTerrain(const FWorldGenSettings& S, UFarmGrid& World)
	{
		// Built once for the whole grid: a tile's rank only exists relative to every other tile.
		TArray<float> Region = BuildRegionMap(S);
		RemapToPercentile(Region);
		ApplyLayoutGuarantee(S, Region);
		
		const TArray<FVector2D> DetailOffsets = BuildOctaveOffsets(S.DetailNoise, S.Seed, Salt::Detail);
		
		const FRichCurve* HeightCurve	 = S.HeightCurve.GetRichCurveConst();
		const FRichCurve* AmplitudeCurve = S.DetailAmplitudeCurve.GetRichCurveConst();
		
		for (int32 Y = 0; Y < UFarmGrid::Height; Y++)
		{
			for (int32 X = 0; X < UFarmGrid::Width; X++)
			{
				const float R	   = Region[World.GetIndex(X, Y)];
				const float Base   = HeightCurve->Eval(R);
				
				const float Bump	  = SampleFbm01(X, Y, S.DetailNoise, DetailOffsets) * 2.f - 1.f;
				const float Amplitude = AmplitudeCurve->Eval(R);
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
	
	OutWorld.ResetWorld();			// always start from a blank world
	GenerateTerrain(S, OutWorld);
	GenerateTrees(S, OutWorld);		// after terrain: trees need tile type
	
	UE_LOG(LogFarm, Log, TEXT("FarmGenerator: world generated from seed %d"), S.Seed);
	OutWorld.LogStats(S.Seed);
}
