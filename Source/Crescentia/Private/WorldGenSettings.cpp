#include "WorldGenSettings.h"

FWorldGenSettings::FWorldGenSettings()
{
	RegionNoise.Frequency  = 0.008f;	// one region spans	roughly 100+ tiles
	RegionNoise.NumOctaves = 2;			// region should be smooth, not detailed
	
	DetailNoise.Frequency  = 0.08f;
	DetailNoise.NumOctaves = 3;
	
	// Linear keys keep flat segments perfectly flat.
	// Auto/cubic tangents can overshoot and add a small bump between two equal keys.
	FRichCurve* Height = HeightCurve.GetRichCurve();
	for (const FVector2f& K: {
		FVector2f(0.00f, 0.05f), FVector2f(0.28f, 0.18f),   // lake bottoms -> shore
		FVector2f(0.33f, 0.30f), FVector2f(0.58f, 0.30f),   // plains (flat, most common)
		FVector2f(0.63f, 0.52f), FVector2f(0.70f, 0.52f),   // plateau (flat top)
		FVector2f(0.75f, 0.75f), FVector2f(1.00f, 0.95f) }) // mountains
	{
		const FKeyHandle Key = Height->AddKey(K.X, K.Y);
		Height->SetKeyInterpMode(Key, RCIM_Linear);
	}
	
	FRichCurve* DetailAmplitude = DetailAmplitudeCurve.GetRichCurve();
	for (const FVector2f& K: {
		FVector2f(0.00f, 0.02f), FVector2f(0.33f, 0.01f),   // lake -> plains: almost flat
		FVector2f(0.58f, 0.01f), FVector2f(0.63f, 0.04f),   // plains end  -> slope
		FVector2f(0.70f, 0.02f), FVector2f(0.75f, 0.08f),   // plateau top -> slope
		FVector2f(1.00f, 0.12f) })							// mountains: roughest
	{
		const FKeyHandle Key = DetailAmplitude->AddKey(K.X, K.Y);
		DetailAmplitude->SetKeyInterpMode(Key, RCIM_Linear);
	}
}
