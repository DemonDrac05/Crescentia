#include "WorldGenSettings.h"

FWorldGenSettings::FWorldGenSettings()
{
	RegionNoise.Frequency  = 0.008f;	// one region spans roughly 100+ tiles
	RegionNoise.NumOctaves = 2;			// region should be smooth, not detailed
	
	DetailNoise.Frequency  = 0.08f;
	DetailNoise.NumOctaves = 3;
	
	// Linear keys keep flat segments perfectly flat.
	// Auto/cubic tangents can overshoot and add a small bump between two equal keys.
	FRichCurve* Height = HeightCurve.GetRichCurve();
	for (const FVector2f& K: {
		// Height: X = share of map (percentile), Y = base height.
		FVector2f(0.00f, 0.05f), FVector2f(0.06f, 0.18f),	// lakes: lowest 6% of the map
		FVector2f(0.10f, 0.30f), FVector2f(0.70f, 0.30f),   // plains: 60% of the map, flat
		FVector2f(0.74f, 0.52f), FVector2f(0.84f, 0.52f),   // plateau tops: 10%, flat
		FVector2f(0.88f, 0.75f), FVector2f(1.00f, 0.95f)})	// mountains: highest 12%
	{
		const FKeyHandle Key = Height->AddKey(K.X, K.Y);
		Height->SetKeyInterpMode(Key, RCIM_Linear);
	}
	
	FRichCurve* DetailAmplitude = DetailAmplitudeCurve.GetRichCurve();
	for (const FVector2f& K: {
		// Detail amplitude: same X meaning.
		FVector2f(0.00f, 0.02f), FVector2f(0.10f, 0.03f),   // lakes -> plains: gentle
		FVector2f(0.70f, 0.03f), FVector2f(0.74f, 0.05f),   // plains end -> slope
		FVector2f(0.84f, 0.03f), FVector2f(0.88f, 0.08f),   // plateau top -> slope
		FVector2f(1.00f, 0.12f)})	                        // mountains: roughest
	{
		const FKeyHandle Key = DetailAmplitude->AddKey(K.X, K.Y);
		DetailAmplitude->SetKeyInterpMode(Key, RCIM_Linear);
	}
}
