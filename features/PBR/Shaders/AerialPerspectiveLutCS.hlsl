#include "Atmosphere.hlsl"

RWTexture3D<float4> AerialPerspectiveLut : register(u0);
Texture2D<float4> TransmittanceLut : register(t0);
Texture2D<float4> MultiScatteringLut : register(t1);

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

[numthreads(2, 32, 1)] void main(uint3 GlobalID
								 : SV_DispatchThreadID) {
	uint width, height, depth;
	AerialPerspectiveLut.GetDimensions(width, height, depth);

	/*float2 uv = float2(GlobalID.xy) / float2(width, height);
    uv = uv * 2 - 1;
    float4 NDCoord = float4(uv, 1, 1);
    float4 WorldCoord = mul(InvViewProjMatrix, NDCoord);
    float3 RayDirection = normalize(WorldCoord.xyz);*/

	float2 uv = float2(GlobalID.xy) / float2(width, height);
	float coordx = uv.x * pi;
	float coordy = uv.y;
	coordy *= coordy;
	coordy = 1.0 - coordy;
	coordy = coordy * ZenithHorizonAngle;

	float z = cos(coordy);
	float lengthxy = sqrt(1 - z * z);
	float x = sin(coordx) * lengthxy;
	float y = cos(coordx) * lengthxy;
	float3 RayDirection = normalize(float3(x, y, z));

	float CosSunUp = SunDirection.z;
	float SinSunUp = sqrt(1.0 - CosSunUp * CosSunUp);
	float CosSunRay = y * SinSunUp + z * CosSunUp;

	float3 SunDirection2 = float3(0, SinSunUp, CosSunUp);

	//float CosSunRay = dot(SunDirection.xyz, RayDirection);

	float3 RayleighPhase2 = rayleighScatteringCoefficient * RayleighPhase(CosSunRay);
	float MiePhase2 = mieScatteringCoefficient * MiePhase(CosSunRay);
	float SampletLength = FogDistance / float(depth);
	float3 ViewTransmittance = 1.0;
	float CurrentDistance = SampletLength * 0.5;
	float3 Luminance = 0.0;

	//float MaxDistance;
	//IntersectDoubleCircleInside(ViewRadius, RayDirection.z, MaxDistance);

	for (int i = 0; i < depth; i++) {
		float3 SamplePosition = float3(0.0, 0.0, ViewRadius) + CurrentDistance * RayDirection;
		float SampleRadius = length(SamplePosition);
		float3 SampleUp = SamplePosition / SampleRadius;
		float SampleCosSunUp = dot(SunDirection2.xyz, SampleUp);
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

		float AverageViewTransmittance = dot(ViewTransmittance * SunTransmittance, float3(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0));
		AerialPerspectiveLut[uint3(GlobalID.xy, i)] = float4(Luminance * float3(1.0, 0.949, 0.937) * SunBrightness, 1 - AverageViewTransmittance);
	}
}