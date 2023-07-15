struct VS_INPUT
{
	float4 Position									: POSITION0;
	float2 TexCoord									: TEXCOORD0;
	float4 Normal									: NORMAL0;
	float4 Color									: COLOR0;
	float4 InstanceData1							: TEXCOORD4;
	float4 InstanceData2							: TEXCOORD5;
	float4 InstanceData3							: TEXCOORD6;
	float4 InstanceData4							: TEXCOORD7;
};

struct VS_OUTPUT
{
	float4 HPosition								: SV_POSITION0;
	float4 VertexColor								: COLOR0;
	float3 TexCoord									: TEXCOORD0;
	float3 ViewSpacePosition						: TEXCOORD1;
#if defined(RENDER_DEPTH)
	float2 Depth									: TEXCOORD2;
#endif
	float4 WorldPosition							: POSITION1;
	float4 PreviousWorldPosition					: POSITION2;
	float3 ViewDirectionVec 						: POSITION3;
	float3 VertexNormal 					        : POSITION4;
};

cbuffer PerGeometry									: register(b2)
{
	row_major float4x4 WorldViewProj				: packoffset(c0);
	row_major float4x4 WorldView					: packoffset(c4);
	row_major float4x4 World						: packoffset(c8);
	row_major float4x4 PreviousWorld				: packoffset(c12);
	float4 FogNearColor								: packoffset(c16);
	float3 WindVector								: packoffset(c17);
	float WindTimer 								: packoffset(c17.w);
	float3 DirLightDirection						: packoffset(c18);
	float PreviousWindTimer 						: packoffset(c18.w);
	float3 DirLightColor							: packoffset(c19);
	float AlphaParam1 								: packoffset(c19.w);
	float3 AmbientColor								: packoffset(c20);
	float AlphaParam2 								: packoffset(c20.w);
	float3 ScaleMask								: packoffset(c21);
	float ShadowClampValue 							: packoffset(c21.w);
}

cbuffer PerFrame : register(b3)
{
	float4  			EyePosition;
	row_major float3x4 	DirectionalAmbient;
	float 				SunlightScale;
	float 				Glossiness;
	float 				SpecularStrength;
	float 				SubsurfaceScatteringAmount;
	bool 				EnableDirLightFix;
	bool 				EnablePointLights;
	float pad0;
	float pad1;
}

cbuffer PerFramePBR : register(b5)
{	
	float4 WorldEyePosition;
	float NonMetalGlossiness;
	float MetalGlossiness;
	float MinRoughness;
	float MaxRoughness;
	float NonMetalThreshold;
	float MetalThreshold;
	float SunShadowAO;
	float ParallaxAO;
	float ParallaxScale;
	float Exposure;
	float GrassRoughness;
	float GrassBentNormal;
	float FogIntensity;
	float AmbientDiffuse;
	float AmbientSpecular;
	float CubemapIntensity;
}

#ifdef VSHADER

#ifdef GRASS_COLLISION
	#include "RunGrass\\GrassCollision.hlsli"
#endif

cbuffer cb7											: register(b7)
{
	float4 cb7[1];
}

cbuffer cb8 : register(b8)
{
	float4 cb8[240];
}


#define M_PI  3.1415925 // PI
#define M_2PI 6.283185 // PI * 2

const static float4x4 M_IdentityMatrix =
{
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 }
};

float4 GetMSPosition(VS_INPUT input, float windTimer, float3x3 world3x3)
{
	float windAngle = 0.4 * ((input.InstanceData1.x + input.InstanceData1.y) * -0.0078125 + windTimer);
	float windAngleSin, windAngleCos;
	sincos(windAngle, windAngleSin, windAngleCos);

	float windTmp3 = 0.2 * cos(M_PI * windAngleCos);
	float windTmp1 = sin(M_PI * windAngleSin);
	float windTmp2 = sin(M_2PI * windAngleSin);
	float windPower = WindVector.z * (((windTmp1 + windTmp2) * 0.3 + windTmp3) *
		(0.5 * (input.Color.w * input.Color.w)));

	float3 inputPosition = input.Position.xyz * (input.InstanceData4.yyy * ScaleMask.xyz + float3(1, 1, 1));
	float3 InstanceData4 = mul(world3x3, inputPosition);

	float3 windVector = float3(WindVector.xy, 0);

	float4 msPosition;
	msPosition.xyz = input.InstanceData1.xyz + (windVector * windPower + InstanceData4);
	msPosition.w = 1;

	return msPosition;
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT vsout;

	float3x3 world3x3 = float3x3(input.InstanceData2.xyz, input.InstanceData3.xyz, float3(input.InstanceData4.x, input.InstanceData2.w, input.InstanceData3.w));

	float4 msPosition = GetMSPosition(input, WindTimer, world3x3);

#ifdef GRASS_COLLISION
	float3 displacement = GetDisplacedPosition(msPosition.xyz, input.Color.w);
	msPosition.xyz += displacement;
#endif

	float4 projSpacePosition = mul(WorldViewProj, msPosition);
	vsout.HPosition = projSpacePosition;

#if defined(RENDER_DEPTH)
	vsout.Depth = projSpacePosition.zw;
#endif

	float perInstanceFade = dot(cb8[(asuint(cb7[0].x) >> 2)].xyzw, M_IdentityMatrix[(asint(cb7[0].x) & 3)].xyzw);
	float distanceFade = 1 - saturate((length(projSpacePosition.xyz) - AlphaParam1) / AlphaParam2);

	// Note: input.Color.w is used for wind speed
	vsout.VertexColor.xyz = input.Color.xyz * input.InstanceData1.www;
	vsout.VertexColor.w = distanceFade * perInstanceFade;

	vsout.TexCoord.xy = input.TexCoord.xy;
	vsout.TexCoord.z = FogNearColor.w;

	vsout.ViewSpacePosition = mul(WorldView, msPosition).xyz;
	vsout.WorldPosition = mul(World, msPosition);

	float4 previousMsPosition = GetMSPosition(input, PreviousWindTimer, world3x3);

#ifdef GRASS_COLLISION
	previousMsPosition.xyz += displacement;
#endif

	vsout.PreviousWorldPosition = mul(PreviousWorld, previousMsPosition);

	vsout.ViewDirectionVec.xyz = EyePosition.xyz - vsout.WorldPosition.xyz;

	// Vertex normal needs to be transformed to world-space for lighting calculations.
	float3 vertexNormal = input.Normal.xyz * 2.0 - 1.0;
	float NdotV = dot(vsout.ViewDirectionVec, vertexNormal);
	if(NdotV < 0){
		vertexNormal = -vertexNormal;
	}
	//插值
	vertexNormal = lerp(vertexNormal, float3(0,0,1), GrassBentNormal);
	//float3 vertexNormal = lerp(input.Normal.xyz * 2.0 - 1.0, float3(0, 0, 1), input.TexCoord.y * input.TexCoord.y * saturate(input.Color.w > 0));
	vertexNormal = mul(world3x3, vertexNormal);
	vsout.VertexNormal.xyz = vertexNormal;

	return vsout;
}
#endif

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
#if defined(RENDER_DEPTH)
	float4 PS										: SV_Target0;
#else
	float4 Albedo									: SV_Target0;
	float2 MotionVectors							: SV_Target1;
	float4 Normal									: SV_Target2;
#endif
};

#ifdef PSHADER
SamplerState SampBaseSampler						: register(s0);
SamplerState SampShadowMaskSampler					: register(s1);

Texture2D<float4> TexBaseSampler					: register(t0);
Texture2D<float4> TexShadowMaskSampler				: register(t1);

cbuffer AlphaTestRefCB								: register(b11)
{
	float AlphaTestRefRS : packoffset(c0);
}

cbuffer PerFrame : register(b12)
{
    row_major float4x4  ViewMatrix                                                  : packoffset(c0);
    row_major float4x4  ProjMatrix                                                  : packoffset(c4);
    row_major float4x4  ViewProjMatrix                                              : packoffset(c8);
    row_major float4x4  ViewProjMatrixUnjittered                                    : packoffset(c12);
    row_major float4x4  PreviousViewProjMatrixUnjittered                            : packoffset(c16);
    row_major float4x4  InvProjMatrixUnjittered                                     : packoffset(c20);
    row_major float4x4  ProjMatrixUnjittered                                        : packoffset(c24);
    row_major float4x4  InvViewMatrix                                               : packoffset(c28);
    row_major float4x4  InvViewProjMatrix                                           : packoffset(c32);
    row_major float4x4  InvProjMatrix                                               : packoffset(c36);
    float4              CurrentPosAdjust                                            : packoffset(c40);
    float4              PreviousPosAdjust                                           : packoffset(c41);
    // notes: FirstPersonY seems 1.0 regardless of third/first person, could be LE legacy stuff
    float4              GammaInvX_FirstPersonY_AlphaPassZ_CreationKitW              : packoffset(c42);
    float4              DynamicRes_WidthX_HeightY_PreviousWidthZ_PreviousHeightW    : packoffset(c43);
    float4              DynamicRes_InvWidthX_InvHeightY_WidthClampZ_HeightClampW    : packoffset(c44);
}

struct StructuredLight
{
	float4  color;
	float4  positionWS;
	float4  positionVS;
	float   radius;
	bool    shadow;
	float   mask;
	bool    active;
};

StructuredBuffer<StructuredLight> lights : register(t17);

float GetSoftLightMultiplier(float angle, float strength)
{
	float softLightParam = saturate((strength + angle) / (1 + strength));
	float arg1 = (softLightParam * softLightParam) * (3 - 2 * softLightParam);
	float clampedAngle = saturate(angle);
	float arg2 = (clampedAngle * clampedAngle) * (3 - 2 * clampedAngle);
	float softLigtMul = saturate(arg1 - arg2);
	return softLigtMul;
}

float3 GetLightSpecularInput(float3 L, float3 V, float3 N, float3 lightColor, float shininess)
{
	float3 H = normalize(V + L);
	float HdotN = saturate(dot(H, N));

	float lightColorMultiplier = exp2(shininess * log2(HdotN));
	return lightColor * lightColorMultiplier.xxx;
}

float3 TransformNormal(float3 normal)
{
	return normal * 2 + -1.0.xxx;
}

// http://www.thetenthplanet.de/archives/1180
float3x3 CalculateTBN(float3 N, float3 p, float2 uv)
{
	// get edge vectors of the pixel triangle
	float3 dp1 = ddx_coarse(p);
	float3 dp2 = ddy_coarse(p);
	float2 duv1 = ddx_coarse(uv);
	float2 duv2 = ddy_coarse(uv);

	// solve the linear system
	float3 dp2perp = cross(dp2, N);
	float3 dp1perp = cross(N, dp1);
	float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = rsqrt(max(dot(T, T), dot(B, B)));
	return float3x3(T * invmax, B * invmax, N);
}

#include "PBR.hlsli"

#if defined(SCREEN_SPACE_SHADOWS)
	#include "ScreenSpaceShadows/ShadowsPS.hlsli"
#endif

PS_OUTPUT main(PS_INPUT input, bool frontFace : SV_IsFrontFace)
{
	PS_OUTPUT psout;

	float x;
	float y;
	TexBaseSampler.GetDimensions(x, y);

	bool complex = x != y;

	float4 baseColor;
	if (complex) {
		baseColor = TexBaseSampler.Sample(SampBaseSampler, float2(input.TexCoord.x, input.TexCoord.y * 0.5));
	}
	else
	{
		baseColor = TexBaseSampler.Sample(SampBaseSampler, input.TexCoord.xy);
	}

	baseColor.xyz *= input.VertexColor.xyz;

#if defined(RENDER_DEPTH) || defined(DO_ALPHA_TEST)
	float diffuseAlpha = input.VertexColor.w * baseColor.w;
	if ((diffuseAlpha - AlphaTestRefRS) < 0)
	{
		discard;
	}
#endif

#if defined(RENDER_DEPTH)
	// Depth
	psout.PS.xyz = input.Depth.xxx / input.Depth.yyy;
	psout.PS.w = diffuseAlpha;
#else

	float4 specColor = complex ? TexBaseSampler.Sample(SampBaseSampler, float2(input.TexCoord.x, 0.5 + input.TexCoord.y * 0.5)) : 0;
	float4 shadowColor = TexShadowMaskSampler.Load(int3(input.HPosition.xy, 0));

	float4 screenPosition = mul(ViewProjMatrixUnjittered, input.WorldPosition);
	screenPosition.xy = screenPosition.xy / screenPosition.ww;
	float4 previousScreenPosition = mul(PreviousViewProjMatrixUnjittered, input.PreviousWorldPosition);
	previousScreenPosition.xy = previousScreenPosition.xy / previousScreenPosition.ww;
	float2 screenMotionVector = float2(-0.5, 0.5) * (screenPosition.xy - previousScreenPosition.xy);

	psout.MotionVectors = screenMotionVector;

	float3 ddx = ddx_coarse(input.ViewSpacePosition);
	float3 ddy = ddy_coarse(input.ViewSpacePosition);
	float3 normal = normalize(cross(ddx, ddy));
	float normalScale = max(1.0 / 1000.0, sqrt(normal.z * -8 + 8));
	psout.Normal.xy = float2(0.5, 0.5) + normal.xy / normalScale;
	psout.Normal.zw = float2(0, 0);

	float3 viewDirection = normalize(input.ViewDirectionVec);
	float3 worldNormal = normalize(input.VertexNormal);

	// Swaps direction of the backfaces otherwise they seem to get lit from the wrong direction.
	//if (!frontFace) worldNormal.xyz = -worldNormal.xyz;

	if (complex) {
		float3 normalColor = float4(TransformNormal(specColor.xyz), 1);
		// Inverting x as well as y seems to look more correct.
		normalColor.xy = -normalColor.xy;
		// world-space -> tangent-space -> world-space. 
		// This is because we don't have pre-computed tangents.
		worldNormal.xyz = normalize(mul(normalColor.xyz, CalculateTBN(worldNormal.xyz, -viewDirection, input.TexCoord.xy)));
	}

	float roughness = saturate(GrassRoughness - specColor.w);
	float2x3 totalRadiance = float2x3(0,0,0,0,0,0);
	float NoV = saturate(dot(worldNormal,viewDirection));
	float3 F0 = 0.04.xxx;

	float3 dirLightColor = DirLightColor.xyz;
	if (EnableDirLightFix)
	{
		dirLightColor *= SunlightScale;
	}

	dirLightColor *= shadowColor.x;

#if defined(SCREEN_SPACE_SHADOWS)
	float dirLightSShadow = PrepassScreenSpaceShadows(input.WorldPosition);
	dirLightColor *= dirLightSShadow;
#endif
	
	totalRadiance += GetLighting(worldNormal, DirLightDirection, viewDirection, NoV, F0, dirLightColor, roughness);

	float shadow = 1;

	if (EnablePointLights) {
		uint light_count, dummy;
		lights.GetDimensions(light_count, dummy);
		for (uint light_index = 0; light_index < light_count; light_index++)
		{
			StructuredLight light = lights[light_index];
			if (light.active) {
				float3 lightDirection = light.positionWS.xyz - input.WorldPosition.xyz;
				float lightDist = length(lightDirection);
				float intensityFactor = saturate(lightDist / light.radius);
				float intensityMultiplier = 1 - intensityFactor * intensityFactor;
				if (intensityMultiplier) {
					float3 lightColor = light.color.xyz * intensityMultiplier;

					if (light.shadow) {
						lightColor *= shadowColor[light.mask];
					}
					
					float3 normalizedLightDirection = normalize(lightDirection);

					totalRadiance += GetLighting(worldNormal, normalizedLightDirection, viewDirection, NoV, F0, lightColor * intensityMultiplier, roughness);
				}
			}
		}
	}

	//环境光漫射
	float ao = saturate(dirLightSShadow + SunShadowAO);
	float3 directionalAmbientColor = mul(DirectionalAmbient, float4(worldNormal.xyz, 1)) * ao;
	totalRadiance[0] += directionalAmbientColor * AmbientDiffuse;

	//环境光反射
	float3 F = FastfresnelSchlick(NoV,F0);
	float G = GeometrySchlickGGX(NoV, roughness);
	G = G * G;
	float3 ambientSpecular = directionalAmbientColor * F * G * AmbientSpecular;
	totalRadiance[1] += ambientSpecular;

	//合并光照
	float3 color = totalRadiance[0] * baseColor.xyz + totalRadiance[1];

	psout.Albedo.xyz = color * Exposure;
	psout.Albedo.w = 1;

#endif
	return psout;
}
#endif