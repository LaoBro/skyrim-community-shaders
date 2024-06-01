#include "Atmosphere.hlsl"

RWTexture2D<float4> MultiScatteringLut : register(u0);
Texture2D<float4> TransmittanceLut : register(t0);

groupshared float3 MultiScatAs1SharedMem[64];
groupshared float3 LuminanceSharedMem[64];

float3 TransmittanceFromTexture(float radius, float angle)
{
	float2 uv = muR2uv(angle, radius, TransmittanceLutResolution);
	return TransmittanceLut.SampleLevel(LinearSampler, uv, 0).xyz;
}

[numthreads(1, 1, 64)] void main(uint3 GlobalID
								 : SV_DispatchThreadID) {
	float2 pixPos = float2(GlobalID.xy) + 0.5f;
	float2 uv = pixPos / MultiScatteringLutResolution;
	//uv.y = 1.0 - uv.y;

	float CosSunUp = uv.x * 2.0 - 1.0;
	float3 SunDirection = float3(sqrt(1.0 - CosSunUp * CosSunUp), CosSunUp, 0.0);

	float ViewRadius = BottomRadius + uv.y * AtmosphereThickness;
	float3 RayOrigin = float3(0.0, ViewRadius, 0.0);

	static const int SampleCount = 32;
	static const int SqrtRayCount = 8;
	static const int RayCount = SqrtRayCount * SqrtRayCount;

	{
		float z = (float(GlobalID.z) + 0.5) / float(RayCount);
		float xyLength = sqrt(1.0 - z * z);
		float Longitude = z * float(SqrtRayCount) * pi * 2.0;
		float3 RayDirection = float3(sin(Longitude) * xyLength, cos(Longitude) * xyLength, z);

		float EndDistance, SampleCosSunUp;
		bool HitGround = IntersectDoubleCircleInside(ViewRadius, RayDirection.y, EndDistance);
		float SampleLength = EndDistance / float(SampleCount);
		float CurrentDistance = SampleLength * 0.5;
		float3 Luminance, MultiScatAs1, SunTransmittance, SampleUp = 0.0;
		float3 ViewTransmittance = 1.0;

		for (int j = 0; j < SampleCount; j++) {
			float3 SamplePosition = RayOrigin + CurrentDistance * RayDirection;
			float SampleRadius = length(SamplePosition);
			float SampleHeight = SampleRadius - BottomRadius;
			float3 SampleDensity = GetDensity(SampleHeight);

			SampleUp = SamplePosition / SampleRadius;
			SampleCosSunUp = dot(SampleUp, SunDirection);
			SunTransmittance = TransmittanceFromTexture(SampleRadius, SampleCosSunUp);
			float3 SampleScattering = rayleighScatteringCoefficient * SampleDensity.x + mieScatteringCoefficient * SampleDensity.y;
			float UniformPhase = 1.0 / (4.0 * pi);
			float3 Inscattering = SunTransmittance * SampleScattering * UniformPhase;

			float3 SampleExtinction = Density2Extinction(SampleDensity);
			float3 SampleTransmittance = Extinction2Transmittance(SampleExtinction, SampleLength);
			float3 NextViewTransmittance = ViewTransmittance * SampleTransmittance;
			float3 IntegralViewTransmittance = (ViewTransmittance - NextViewTransmittance) / SampleExtinction;

			Luminance += IntegralViewTransmittance * Inscattering;
			MultiScatAs1 += IntegralViewTransmittance * SampleScattering;
			CurrentDistance += SampleLength;
			ViewTransmittance = NextViewTransmittance;
		}

		Luminance += HitGround * ViewTransmittance * SunTransmittance * (earthAlbedo / pi) * SampleCosSunUp;

		MultiScatAs1SharedMem[GlobalID.z] = MultiScatAs1;
		LuminanceSharedMem[GlobalID.z] = Luminance;
	}

	GroupMemoryBarrierWithGroupSync();

	// 64 to 32
	if (GlobalID.z < 32) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 32];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 32];
	}
	GroupMemoryBarrierWithGroupSync();

	// 32 to 16
	if (GlobalID.z < 16) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 16];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 16];
	}
	GroupMemoryBarrierWithGroupSync();

	// 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
	if (GlobalID.z < 8) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 8];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 8];
	}
	GroupMemoryBarrierWithGroupSync();
	if (GlobalID.z < 4) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 4];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 4];
	}
	GroupMemoryBarrierWithGroupSync();
	if (GlobalID.z < 2) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 2];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 2];
	}
	GroupMemoryBarrierWithGroupSync();
	if (GlobalID.z < 1) {
		MultiScatAs1SharedMem[GlobalID.z] += MultiScatAs1SharedMem[GlobalID.z + 1];
		LuminanceSharedMem[GlobalID.z] += LuminanceSharedMem[GlobalID.z + 1];
	}
	GroupMemoryBarrierWithGroupSync();
	if (GlobalID.z > 0)
		return;

	float3 MultiScatAs1 = MultiScatAs1SharedMem[0];
	float3 InScatteredLuminance = LuminanceSharedMem[0];

	float3 L = InScatteredLuminance / (float(RayCount) - MultiScatAs1);

	MultiScatteringLut[GlobalID.xy] = float4(L, 1.0);
}