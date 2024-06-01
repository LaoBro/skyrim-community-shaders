#include "PBRConstantBuffer.hlsl"

Texture2D<float4> SkyViewLutSRV : register(t41);
SamplerState LinearSampler : register(s15);

#define TransmittanceLutResolution int2(256, 64)
#define MultiScatteringLutResolution int2(32, 32)
#define SkyViewLutResolution int2(128, 128)

#define pi 3.141592
// Use Megametre as units, so scattering and absorption Coefficient are 10^6 time larger than actual
#define BottomRadius 6360e3
#define AtmosphereThickness 100e3
static const float TopRadius = BottomRadius + AtmosphereThickness;
static const float BottomRadius2 = BottomRadius * BottomRadius;
static const float TopRadius2 = TopRadius * TopRadius;
static const float H = sqrt(TopRadius2 - BottomRadius2);
#define rayleighScaleHeight 8e3
#define mieScaleHeight 1.2e3
#define rayleighScatteringCoefficient float3(5.8e-6, 13.5e-6, 33.1e-6)
#define mieScatteringCoefficient 3.996e-6
#define mieAbsorptionCoefficient 4.440e-6
#define mieExtinctionCoefficient 8.436e-6
#define ozoneCenter 25e3
#define ozoneWidth 15e3
#define ozoneAbsorptionCoefficient float3(0.650e-6, 1.881e-6, 0.085e-6)
#define earthAlbedo 0.3
#define SunCosAngle 0.9995

bool IntersectCircle(float ViewRadius, float CosAngle, out float StartDistance, out float EndDistance)
{
	float Offset = -ViewRadius * CosAngle;
	float ViewRadius2 = ViewRadius * ViewRadius;
	float Ray2Center2 = ViewRadius2 - Offset * Offset;
	if (Ray2Center2 > TopRadius2) {
		return false;
	}
	float TopHalfLength = sqrt(TopRadius2 - Ray2Center2);
	EndDistance = TopHalfLength + Offset;
	StartDistance = max(0.0, Offset - TopHalfLength);
	return true;
}

void IntersectCircleInside(float ViewRadius, float CosAngle, out float TriangleHeight2, out float StartWidth, out float EndWidth)
{
	StartWidth = ViewRadius * CosAngle;
	float ViewRadius2 = ViewRadius * ViewRadius;
	TriangleHeight2 = ViewRadius2 - StartWidth * StartWidth;
	EndWidth = sqrt(TopRadius2 - TriangleHeight2);
}

void IntersectDoubleCircle(float ViewRadius, float CosAngle, out float StartDistance, out float EndDistance)
{
	float Offset = -ViewRadius * CosAngle;
	float ViewRadius2 = ViewRadius * ViewRadius;
	float Ray2Center2 = ViewRadius2 - Offset * Offset;
	float BottomHalfLength = sqrt(BottomRadius2 - Ray2Center2);
	float TopHalfLength = sqrt(TopRadius2 - Ray2Center2);
	StartDistance = max(0.0, Offset - TopHalfLength);
	EndDistance = Offset - BottomHalfLength;
}

bool IntersectDoubleCircleInside(float ViewRadius, float CosAngle, out float EndDistance)
{
	float Offset = -ViewRadius * CosAngle;
	float ViewRadius2 = ViewRadius * ViewRadius;
	float Ray2Center2 = ViewRadius2 - Offset * Offset;
	if (Ray2Center2 < BottomRadius2 && CosAngle < 0.0) {
		float BottomHalfLength = sqrt(BottomRadius2 - Ray2Center2);
		EndDistance = Offset - BottomHalfLength;
		return true;
	} else {
		float TopHalfLength = sqrt(TopRadius2 - Ray2Center2);
		EndDistance = TopHalfLength + Offset;
		return false;
	}
}

float3 GetDensity(float height)
{
	float3 Density = float3(exp(-height / float2(rayleighScaleHeight, MieHeight)), (1.0 - min(abs(height - ozoneCenter) / ozoneWidth, 1.0)));
	return Density * float3(RayleighScattering, MieScattering, OzoneAbsorption);
}

float3 Density2Extinction(float3 Density)
{
	return rayleighScatteringCoefficient * Density.x + mieExtinctionCoefficient * Density.y + ozoneAbsorptionCoefficient * Density.z;
}

float3 Extinction2Transmittance(float3 Extinction, float SegmentLength)
{
	return exp(-Extinction * SegmentLength);
}

float RayleighPhase(float angle)
{
	return 3.0 * (1.0 + (angle * angle)) / (16.0 * pi);
}

float MiePhase(float angle)
{
	const float g = MieAnisotropy;
	const float g2 = g * g;
	float Alpha = 1.0 + g2 - 2.0 * g * angle;
	return 3.0 * (1.0 - g2) * (1.0 + angle * angle) / (8.0 * pi * (2.0 + g2) * Alpha * sqrt(Alpha));
}

float distToExitAtmosphere(float mu, float r)
{
	float discriminant = r * r * (mu * mu - 1.0) + TopRadius2;
	return max(-r * mu + sqrt(max(discriminant, 0.)), 0.);
}

float unitRange2texCoord(float x, float tex_size)
{
	return (0.5 + x * (tex_size - 1)) / tex_size;
}

float texCoord2unitRange(float u, float resolution)
{
	return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

void ID2muR(float2 ID, out float mu, out float r, float2 tex_size)
{
	float x_mu = ID.x / (tex_size.x - 1);
	float x_r = ID.y / (tex_size.y - 1);
	float rho = H * x_r;
	r = sqrt(rho * rho + BottomRadius2);
	float d_min = TopRadius - r;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	mu = d == 0.0 ? 1. : (H * H - rho * rho - d * d) / (2. * r * d);
	mu = clamp(mu, -1., 1.);
}

float2 muR2uv(float mu, float r, float2 tex_size)
{
	float rho = sqrt(r * r - BottomRadius2);
	float d = distToExitAtmosphere(mu, r);
	float d_min = TopRadius - r;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	return float2(
		unitRange2texCoord(x_mu, tex_size.x),
		unitRange2texCoord(x_r, tex_size.y));
}

float sunWithBloom(float3 rayDir, float3 sunDir)
{
	const float sunSolidAngle = 0.53 * pi / 180.0;
	//const float minSunCosTheta = cos(sunSolidAngle);
	const float minSunCosTheta = SunCosAngle;

	float cosTheta = dot(rayDir, sunDir);
	if (cosTheta >= minSunCosTheta)
		return float(1.0);

	float offset = minSunCosTheta - cosTheta;
	float gaussianBloom = exp(-offset * 50000.0) * 0.5;
	float invBloom = 1.0 / (0.02 + offset * 300.0) * 0.01;
	return gaussianBloom + invBloom;
}

float3 SampleSkyViewLut(float3 WorldPosition)
{
	float3 RayDirection = normalize(WorldPosition);

	float3 Up = float3(0, 0, 1);
	float3 Right = cross(SunDirection.xyz, Up);
	float3 Forward = cross(Up, Right);

	float CosRayUp = dot(RayDirection, Up);
	float RayUpRadian = acos(CosRayUp);

	float u, v;

	v = RayUpRadian;
	v /= ZenithHorizonAngle;
	v = 1.0 - v;
	v = saturate(v);
	v = sqrt(v);
	v = saturate(v);

	float CosRayForward = dot(RayDirection, Forward);
	float CosRayRight = dot(RayDirection, Right);
	float RayForwardRadian = acos(CosRayForward / sqrt(CosRayRight * CosRayRight + CosRayForward * CosRayForward));

	u = saturate(RayForwardRadian / pi);
	float2 uv = float2(unitRange2texCoord(u, SkyViewLutResolution.x), unitRange2texCoord(v, SkyViewLutResolution.y));

	float4 LutColor = SkyViewLutSRV.SampleLevel(LinearSampler, uv, 0);
	float3 Color = LutColor.xyz;

	Color += SunColor.xyz * LutColor.w * sunWithBloom(RayDirection, SunDirection.xyz) / pi;

	return Color;
}
