#include "Atmosphere.hlsl"

RWTexture2D<float4> SkyViewLut : register(u0);
Texture2D<float4> TransmittanceLut : register(t0);
Texture2D<float4> MultiScatteringLut : register(t1);

static const int SampleCount = 32;

float3 TransmittanceFromTexture(float radius, float angle)
{
	float2 uv = muR2uv(angle, radius, TransmittanceLutResolution);
	return TransmittanceLut.SampleLevel(LinearSampler, uv, 0).xyz;
}

float3 MultipleScatteringContributionFromTexture(float height, float angle)
{
	float u = (angle + 1.0) * 0.5;
	float v = height / AtmosphereThickness;
	return MultiScatteringLut.SampleLevel(LinearSampler, float2(u, v), 0).xyz;
}

[numthreads(1, 64, 1)] void main(uint3 GlobalID
								 : SV_DispatchThreadID) {
	float StartDistance, EndDistance;

	float2 uv = float2(GlobalID.xy) / float2(SkyViewLutResolution);
	float coordx = uv.x * pi;
	float coordy = uv.y;
	coordy *= coordy;
	coordy = 1.0 - coordy;

	coordy = coordy * ZenithHorizonAngle;
	IntersectCircle(ViewRadius, cos(coordy), StartDistance, EndDistance);

	float z = cos(coordy);
	float lengthxy = sqrt(1 - z * z);
	float x = sin(coordx) * lengthxy;
	float y = cos(coordx) * lengthxy;
	float3 RayDirection = normalize(float3(x, y, z));

	float CosSunUp = SunDirection.z;

	float SinSunUp = sqrt(1.0 - CosSunUp * CosSunUp);
	float CosSunRay = y * SinSunUp + z * CosSunUp;

	float3 SunDirection2 = float3(0, SinSunUp, CosSunUp);

	float3 RayleighPhase2 = rayleighScatteringCoefficient * RayleighPhase(CosSunRay);
	float MiePhase2 = mieScatteringCoefficient * MiePhase(CosSunRay);
	float SampletLength = (EndDistance - StartDistance) / float(SampleCount);
	float3 ViewTransmittance = 1.0;
	float CurrentDistance = SampletLength * 0.5 + StartDistance;
	float3 Luminance = 0.0;

	for (int i = 0; i < SampleCount; i++) {
		float3 SamplePosition = float3(0.0, 0.0, ViewRadius) + CurrentDistance * RayDirection;
		float SampleRadius = length(SamplePosition);
		float3 SampleUp = SamplePosition / SampleRadius;
		float SampleCosSunUp = dot(SunDirection2, SampleUp);
		float SampleHeight = SampleRadius - BottomRadius;
		float3 SampleDensity = GetDensity(SampleHeight);

		float3 SunTransmittance = TransmittanceFromTexture(SampleRadius, SampleCosSunUp);
		float3 Inscattering = SunTransmittance * (RayleighPhase2 * SampleDensity.x + MiePhase2 * SampleDensity.y);
		Inscattering += MultipleScatteringContributionFromTexture(SampleHeight, SampleCosSunUp) * (rayleighScatteringCoefficient * SampleDensity.x + mieScatteringCoefficient * SampleDensity.y);

		float3 SampleExtinction = Density2Extinction(SampleDensity);
		float3 SampleTransmittance = Extinction2Transmittance(SampleExtinction, SampletLength);
		float3 NextViewTransmittance = ViewTransmittance * SampleTransmittance;
		float3 IntegralViewTransmittance = (ViewTransmittance - NextViewTransmittance) / SampleExtinction;

		Luminance += IntegralViewTransmittance * Inscattering;
		CurrentDistance += SampletLength;
		ViewTransmittance = NextViewTransmittance;
	}

	float3 Color = Luminance * float3(1.0, 0.949, 0.937) * SunBrightness;
	SkyViewLut[GlobalID.xy] = float4(Color, ViewTransmittance.x);
}