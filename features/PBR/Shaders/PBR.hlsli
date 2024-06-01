#include "PBRConstantBuffer.hlsl"

//--------------------------------------------------------------------------------
//Settings
//--------------------------------------------------------------------------------

#define Geometry_Function 2  //[1-2]
// 1 = Google Filament
// 2 = UE4(fast)

#define Sheen_Model 1  //[1-2]
// 1 = CharlieD
// 2 = Ashikhmin

#define Point_Light_Attenuation 1  //[1-3]
// 1 = Vanilla
// 2 = Inverse Proportional
// 3 = Unity

//--------------------------------------------------------------------------------
//Define
//--------------------------------------------------------------------------------

#if defined(FACEGEN) || defined(FACEGEN_RGB_TINT) || defined(SOFT_LIGHTING)
#	define SSS
#endif

static const float PI = 3.14159265;
static const float MinimalPrecision = 1e-4;

Texture2D<float4> WaterCubemap : register(t43);

//--------------------------------------------------------------------------------
//BaseFunction
//--------------------------------------------------------------------------------

//UE4 菲尼尔
//UE4 Fresnel function
float3 FastfresnelSchlick(float LoH, float3 F0)
{
	return F0 + (1 - F0) * exp2((-5.55473 * LoH - 6.98316) * LoH);
}

float FastfresnelSchlick(float LoH, float F0)
{
	return F0 + (1 - F0) * exp2((-5.55473 * LoH - 6.98316) * LoH);
}

//UE4 几何遮蔽倒数
//UE4 geometry function invert
float Vis_SmithJointApprox_invert(float NoV, float NoL, float a)
{
	float Vis_SmithV = NoL * (NoV * (1 - a) + a);
	float Vis_SmithL = NoV * (NoL * (1 - a) + a);
	return Vis_SmithV + Vis_SmithL;
}

float Vis_SmithJointApprox_invert2(float NoVpNoL, float NoVmNoL, float a)
{
	return lerp(2 * NoVmNoL, NoVpNoL, a);
}

//Google Filament 几何遮蔽倒数
//Google Filament geometry function invert
float V_SmithGGXCorrelated_invert(float NoV, float NoL, float a2)
{
	float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
	float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
	return GGXV + GGXL;
}

//blender GGX倒数
//比原版少除了一个PI
float bxdf_ggx_D_invert(float NoH, float a2)
{
	float tmp = (NoH * a2 - NoH) * NoH + 1.0;
	/* Doing RCP and mul a2 at the end. */
	return tmp * tmp;
}

float gScatter(float NoV, float NoL, float d)
{
	float NoVmNoL = NoV * NoL + MinimalPrecision;
	float NoVpNoL = NoV + NoL + MinimalPrecision;
	float numerator = 1 - exp(-d * NoVpNoL / NoVmNoL);
	float denominator = NoVpNoL;
	return numerator / denominator;
}

float D_SGGX_invert(float NoH, float a2)
{
	float NoH2 = NoH * NoH;
	float tmp = lerp(a2, 1, NoH2);
	return tmp * tmp;
	/* Doing RCP and mul a3 at the end. */
}

float G_SGGX_invert(float NoL, float a2)
{
	float NoL2 = NoL * NoL;
	float tmp = lerp(1, a2, NoL2);
	return sqrt(tmp);
	/* Doing RCP and mul 0.25 at the end. */
}

float G_SGGX_invert2(float NoL, float a)
{
	float NoL2 = NoL * NoL;
	float tmp = lerp(1, a, NoL2);
	return tmp;
	/* Doing RCP and mul 0.25 at the end. */
}

float D_InvGGX(float NoH, float a2)
{
	float A = 4;
	float d = (NoH - a2 * NoH) * NoH + a2;
	return rcp(1 + A * a2) * (1 + 4 * a2 * a2 / (d * d));
}

float GetCLothScatter(float NoV, float NoL, float NoH, float a, float a2, float d)
{
	float NoVmNoL = max(NoV * NoL, MinimalPrecision);
	float NoVpNoL = max(NoV + NoL, MinimalPrecision);
	float numerator = 1 - exp(-d * NoVpNoL / NoVmNoL);
	float denominator = D_SGGX_invert(NoH, a2) * G_SGGX_invert(NoL, a2);
	//float denominator = D_SGGX_invert(NoH, a2);
	denominator *= NoVpNoL;
	return numerator / (denominator + MinimalPrecision);
}

float F_Schlick(float u, float F0, float F90)
{
	return F0 + (F90 - F0) * exp2((-5.55473 * u - 6.98316) * u);
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness)
{
	float F90 = 0.5 + 2.0 * roughness * LoH * LoH;
	float lightScatter = F_Schlick(NoL, 1.0, F90);
	float viewScatter = F_Schlick(NoV, 1.0, F90);
	return lightScatter * viewScatter;
}

//地平线高光遮蔽
//Horizon specular occlusion
float HorizonSpecularOcclusion(float3 r, float3 n)
{
	float horizon = min(1.0 + dot(r, n), 1.0);
	return horizon * horizon;
}

float SGIrradianceFitted(in float Sharpness, in float NoL)
{
	const float lambda = Sharpness;

	const float c0 = 0.36f;
	const float c1 = 1.0f / (4.0f * c0);

	float eml = exp(-lambda);
	float em2l = eml * eml;
	float rl = rcp(lambda);

	float scale = 1.0f + 2.0f * em2l - rl;
	float bias = (eml - em2l) * rl - em2l;

	float x = sqrt(1.0f - scale);
	float x0 = c0 * NoL;
	float x1 = c1 * x;

	float n = x0 + x1;

	float y = saturate(NoL);
	if (abs(x0) <= x1)
		y = n * n / x;

	float result = scale * y + bias;

	return result;
}

float3 GetSGSoftLight(float3 ScatterAmt, float NoL, float NoL2, float Curvature)
{
	float3 NoL3 = lerp(NoL, NoL2, saturate(ScatterAmt));
	float3 Sharpness = 1 / max(ScatterAmt * Curvature * SSSAmount, 0.0001f);

	// Compute the irradiance that would result from convolving a punctual light source
	// with the SG filtering kernels
	float3 Diffuse = float3(SGIrradianceFitted(Sharpness.x, NoL3.x),
		SGIrradianceFitted(Sharpness.y, NoL3.y),
		SGIrradianceFitted(Sharpness.z, NoL3.z));

	return Diffuse;
}

float3 EnvBRDFApprox(float3 F0, float Roughness, float NoV)

{
	const float4 c0 = { -1, -0.0275, -0.572, 0.022 };

	const float4 c1 = { 1, 0.0425, 1.04, -0.04 };

	float4 r = Roughness * c0 + c1;

	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;

	float2 AB = float2(-1.04, 1.04) * a004 + r.zw;

	return F0 * AB.x + AB.y;
}

float EnvBRDFApproxNonmetal(float Roughness, float NoV)

{
	const float2 c0 = { -1, -0.0275 };

	const float2 c1 = { 1, 0.0425 };

	float2 r = Roughness * c0 + c1;

	return min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
}

//--------------------------------------------------------------------------------
//Struct
//--------------------------------------------------------------------------------

struct PBRStruct
{
	float3 DiffuseColor;
	float3 F0;
	float Roughness;
	float AO;
	float a;
	float a2;
	float3 N;
	float3 FaceN;
	float3 V;
	float3 R;
	float NoV;
	float3 DiffuseLighting;
	float3 SpecularLighting;
	float3 SSSColor;
	float Curvature;
	bool CubeMap;
	float Specular;
};

PBRStruct GetPBRStruct(float3 Albedo, float3 F0, float Metallic, float Roughness, float3 N, float3 FaceN, float3 V, bool CubeMap)
{
	PBRStruct PBRstruct;

	Albedo = max(0.04, Albedo);

	if (CubeMap) {
		PBRstruct.DiffuseColor = Albedo;
		PBRstruct.F0 = F0;
	} else {
		PBRstruct.DiffuseColor = Albedo * (1 - Metallic);
		PBRstruct.F0 = lerp(0.04, Albedo, Metallic);
	}

	Roughness = max(Roughness, 0.089);
	PBRstruct.Roughness = Roughness;
	float a = Roughness * Roughness;
	PBRstruct.a = a;
	PBRstruct.a2 = a * a;

	PBRstruct.N = N;
	PBRstruct.FaceN = FaceN;
	PBRstruct.V = V;
	PBRstruct.R = reflect(-V, N);
	float NoV = saturate(dot(N, V));
	PBRstruct.NoV = NoV;
	PBRstruct.AO = 1;
	PBRstruct.DiffuseLighting = 0;
	PBRstruct.SpecularLighting = 0;
	PBRstruct.SSSColor = 0;
	PBRstruct.Curvature = 1;
	PBRstruct.CubeMap = CubeMap;

	float SunSpecular = saturate(Outdoor + IndoorSunSpecular + Metallic + CubeMap);
#if defined(SHADOW_DIR)
	SunSpecular = 1;
#endif  // defined (SHADOW_DIR)
	PBRstruct.Specular = SunSpecular;

	if (IsCloth && EnableClothShader) {
		//PBRstruct.DiffuseColor *= FastfresnelSchlick(NoV, ClothDiffuse);
		//PBRstruct.DiffuseColor *= lerp(1, ClothDiffuse, NoV);
	}

	return PBRstruct;
}

struct GrassPBRStruct
{
	float3 DiffuseColor;
	float F0;
	float Sheen;
	float Roughness;
	//float AO;
	float a;
	float a2;
	float3 N;
	float3 V;
	float NoV;
	float3 DiffuseLighting;
	float3 SpecularLighting;
};

GrassPBRStruct GetGrassPBRStruct(float3 BaseColor, float Specular, float3 N, float3 V, float3 InputPosition, bool IsGrass)
{
	GrassPBRStruct PBRstruct;
	float3 ddx = ddx_coarse(InputPosition);
	float3 ddy = ddy_coarse(InputPosition);
	float3 normal = normalize(cross(ddx, ddy));
	float Sheen = 1 - abs(normal.z);
	Sheen = saturate(IsGrass * Sheen * 2);
	PBRstruct.Sheen = Sheen;

	float NoV = saturate(dot(N, V));
	float OcclusionFactor = 0;
	//PBRstruct.DiffuseColor = BaseColor.xyz * FastfresnelSchlick(NoV, GrassOcclusion);
	PBRstruct.DiffuseColor = BaseColor.xyz * lerp(1, GrassBrightness, IsGrass);
	//PBRstruct.AO = saturate(length(InputPosition) / 50 + 1 - Sheen * GrassOcclusion);
	PBRstruct.F0 = 0.04;
	float Roughness = clamp((1 - Specular) * FoliageRoughness, 0.1, 1);
	Roughness = lerp(Roughness, GrassRoughness, Sheen);
	PBRstruct.Roughness = Roughness;
	float a = Roughness * Roughness;
	PBRstruct.a = a;
	PBRstruct.a2 = a * a;

	PBRstruct.N = N;
	PBRstruct.V = V;
	PBRstruct.NoV = NoV;
	PBRstruct.DiffuseLighting = 0;
	PBRstruct.SpecularLighting = 0;

	return PBRstruct;
}

void GetLLFLighting(float3 N, float3 L, float3 V, float3 originalRadiance, inout PBRStruct PBRstruct)
{
	float a = PBRstruct.a;
	float a2 = PBRstruct.a2;
	float NoV = PBRstruct.NoV;
	float3 F0 = PBRstruct.F0;
	//float Sheen = PBRstruct.Sheen;
	float Roughness = PBRstruct.Roughness;
	float3 FaceN = PBRstruct.FaceN;

	float3 H = normalize(V + L);
	float NoL1 = dot(N, L);      //not saturated
	float NoL2 = dot(FaceN, L);  //not saturated
	float NoL = saturate(NoL1);

	float NoVpNoL = NoV + NoL;
	float NoVmNoL = NoV * NoL;

#if defined(SSS)
	float3 DiffuseNoL = GetSGSoftLight(PBRstruct.SSSColor, NoL1, NoL2, PBRstruct.Curvature);
#else
	float DiffuseNoL = NoL;
#endif

	float LoH = saturate(dot(L, H));
	float NoH = saturate(dot(N, H));

	if (EnableClothShader && IsCloth) {
		float ClothScatterWeight = gScatter(NoV, NoL, ClothScatterDensity) * ClothScatterBrightness;
		//float ClothScatterWeight = GetCLothScatter(NoV, NoL, NoH, a, a2, ClothScatterDensity);
		//float D = D_InvGGX(NoH, ClothScatterDensity);
		//float Vis = Vis_SmithJointApprox_invert2(NoVpNoL, NoVmNoL, 2) + MinimalPrecision;
		//float F = FastfresnelSchlick(LoH, ClothScatterBrightness);
		//float kS = D * ClothScatterBrightness / Vis;
		PBRstruct.DiffuseLighting += originalRadiance * saturate(NoL1 + 0.5) * lerp(1, ClothDiffuse, NoV) / 1.5;
		PBRstruct.SpecularLighting += originalRadiance * NoL * ClothScatterWeight;

	} else {
		float D = bxdf_ggx_D_invert(NoH, a2);
		float Vis;
#if Geometry_Function == 1
		Vis = V_SmithGGXCorrelated_invert(NoV, NoL, a2);
#elif Geometry_Function == 2
		Vis = Vis_SmithJointApprox_invert(NoV, NoL, a);
#endif
		float3 F = FastfresnelSchlick(LoH, F0);

		float3 numerator = a2 * F * 0.5;
		float denominator = D * Vis;
		float3 kS = numerator / (denominator + MinimalPrecision);

		PBRstruct.DiffuseLighting += originalRadiance * DiffuseNoL * Fd_Burley(NoV, NoL, LoH, a);
		PBRstruct.SpecularLighting += kS * originalRadiance * NoL * PBRstruct.Specular;
	}

	PBRstruct.Specular = 1;
}

//完整光照模型
//Total Illumination model
void GetLighting(float3 L, float3 originalRadiance, inout PBRStruct PBRstruct)
{
	GetLLFLighting(PBRstruct.N, L, PBRstruct.V, originalRadiance, PBRstruct);
}

//草地光照模型
//grass Illumination model
void GetGrassLighting(float3 L, float3 originalRadiance, inout GrassPBRStruct PBRstruct)
{
	float3 V = PBRstruct.V;
	float3 N = PBRstruct.N;
	float a = PBRstruct.a;
	float a2 = PBRstruct.a2;
	float NoV = PBRstruct.NoV;
	float Roughness = PBRstruct.Roughness;
	float Sheen = PBRstruct.Sheen;
	float F0 = PBRstruct.F0;

	float3 H = normalize(V + L);
	float NoL = saturate(dot(N, L));
	float LoH = saturate(dot(L, H));
	float NoH = saturate(dot(N, H));
	float NoVpNoL = NoV + NoL;
	float NoVmNoL = NoV * NoL;
	//float NsinL = sqrt(1 - NoL * NoL);
	//NsinL = NsinL * 0.5 + 0.5;

	//草地都是非金属
	float D = bxdf_ggx_D_invert(NoH, a2);
	float Vis = Vis_SmithJointApprox_invert2(NoVpNoL, NoVmNoL, a);
	float F = FastfresnelSchlick(LoH, F0);

	float numerator = a2 * 0.5 * F;
	float denominator = D * Vis;

	float kS = numerator / (denominator + MinimalPrecision);

	float3 irradiance = originalRadiance * NoL;

	//PBRstruct.DiffuseLighting += irradiance * GrassOcclusion / (Vis_SmithJointApprox_invert2(NoVpNoL, NoVmNoL, 1) + MinimalPrecision);
	PBRstruct.DiffuseLighting += irradiance * Fd_Burley(NoV, NoL, LoH, a);
	PBRstruct.SpecularLighting += irradiance * kS;
}

void GetAmbientLighting(float3x4 DirectionalAmbient, inout PBRStruct PBRstruct)
{
	float4 N = float4(PBRstruct.N, 1);
	float3 AmbientDiffuseColor = mul(DirectionalAmbient, N);
	if (IsCloth && EnableClothShader) {
		//float ambientDiffuseWeight = F_Schlick(PBRstruct.NoV, 1, 1.5);
		PBRstruct.DiffuseLighting += AmbientDiffuseColor * PBRstruct.AO * AmbientDiffuse;
	} else {
		float Roughness = PBRstruct.Roughness;
		float3 F0 = PBRstruct.F0;
		PBRstruct.DiffuseLighting += AmbientDiffuseColor * PBRstruct.AO * AmbientDiffuse;
		float3 ambientSpecularWeight = EnvBRDFApprox(F0, Roughness, PBRstruct.NoV) * HorizonSpecularOcclusion(PBRstruct.R, PBRstruct.FaceN);
		PBRstruct.SpecularLighting += mul(DirectionalAmbient, float4(PBRstruct.R, 1)) * ambientSpecularWeight * PBRstruct.AO * AmbientSpecular;
	}
}
void GetCubeMapAmbientLighting(float3x4 DirectionalAmbient, float3 CubeMapColor, inout PBRStruct PBRstruct)
{
	PBRstruct.DiffuseColor += CubeMapColor;
	float4 N = float4(PBRstruct.N, 1);
	float3 AmbientDiffuseColor = mul(DirectionalAmbient, N);
	PBRstruct.DiffuseLighting += AmbientDiffuseColor;
}

void GetGrassAmbientLighting(float3x4 DirectionalAmbient, inout GrassPBRStruct PBRstruct)
{
	float3 reflectVector = reflect(-PBRstruct.V, PBRstruct.N);
	float3 AmbientDiffuseColor = mul(DirectionalAmbient, float4(reflectVector, 1));
	PBRstruct.DiffuseLighting += AmbientDiffuseColor * AmbientDiffuse;
	float ambientSpecularWeight = EnvBRDFApproxNonmetal(PBRstruct.Roughness, PBRstruct.NoV);
	PBRstruct.SpecularLighting += AmbientDiffuseColor * ambientSpecularWeight * AmbientSpecular;
}

float3 GetColor(PBRStruct PBRstruct)
{
	return PBRstruct.DiffuseLighting * PBRstruct.DiffuseColor + PBRstruct.SpecularLighting;
}

float3 GetGrassColor(GrassPBRStruct PBRstruct)
{
	return PBRstruct.DiffuseLighting * PBRstruct.DiffuseColor + PBRstruct.SpecularLighting;
}

#define TransmittanceLutResolution int2(256, 64)
#define MultiScatteringLutResolution int2(32, 32)
#define AerialPerspectiveLutResolution int3(32, 32, 32)
#define SkyViewLutResolution int2(128, 128)
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
#define ozoneWidth 25e3
#define ozoneAbsorptionCoefficient float3(0.650e-6, 1.881e-6, 0.085e-6)
#define earthAlbedo 0.3
#define SunCosAngle 0.9995
Texture3D<float4> AerialPerspectiveLutSRV : register(t42);

float unitRange2texCoord(float x, float tex_size)
{
	return (0.5 + saturate(x) * (tex_size - 1)) / tex_size;
}

float3 ApproxScattering(float3 ScreenPosition, float3 WorldPosition, float3 Color, SamplerState LinearSampler)
{
	if (!Outdoor) {
		return Color;
	}

	float z = length(WorldPosition) * 1.428 / (FogDistance);
	z = unitRange2texCoord(z, 32);
	float2 xy = ScreenPosition.xy;
	xy.x /= 1920;
	xy.y /= 1080;
	xy.y = 1 - xy.y;
	xy = float2(unitRange2texCoord(xy.x, 32), unitRange2texCoord(xy.y, 32));

	float3 xyz = float3(xy, z);

	float4 LutColor = AerialPerspectiveLutSRV.SampleLevel(LinearSampler, saturate(xyz), 0);

	Color *= 1 - LutColor.w;
	Color += LutColor.xyz;

	return Color;
}

float3 SampleAerialPerspectiveLut(float3 WorldPosition, float3 Color, SamplerState LinearSampler)
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

	float CosRayForward = dot(RayDirection, Forward);
	float CosRayRight = dot(RayDirection, Right);
	float RayForwardRadian = acos(CosRayForward / sqrt(CosRayRight * CosRayRight + CosRayForward * CosRayForward));

	u = saturate(RayForwardRadian / PI);
	float2 uv = float2(unitRange2texCoord(u, AerialPerspectiveLutResolution.x), unitRange2texCoord(v, AerialPerspectiveLutResolution.y));
	float z = length(WorldPosition) * WorldScale / FogDistance;
	z = unitRange2texCoord(z, AerialPerspectiveLutResolution.z);

	float4 LutColor = AerialPerspectiveLutSRV.SampleLevel(LinearSampler, float3(uv, z), 0);
	Color *= 1 - LutColor.w;
	Color += LutColor.xyz;

	/*float3 RayDirection = normalize(WorldPosition);
    WorldPosition *= WorldScale;
    float2 heightdata = float2(WorldPosition.z + ViewHeight, ViewHeight);
    float2 expdata = exp(-heightdata / FogHeight);

    float Density = (expdata.y - expdata.x) / WorldPosition.z;
    Density = abs(Density);
    Density = max(Density, min(expdata.x, expdata.y));
    Density = min(Density, max(expdata.x, expdata.y));
    Density *= length(WorldPosition) * MieScattering;
    Color = lerp(0.6, Color, exp(-Density));*/

	return Color;
}
