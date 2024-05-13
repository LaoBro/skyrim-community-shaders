cbuffer PBRPerFrame : register(b6)
{	
    float4x4 InvViewProjMatrix;
    row_major float3x4 WorldDirectionalAmbient;
    bool IndoorSunSpecular;
    bool EnableClothShader;
    float ClothDiffuse;
    float ClothScatterDensity;
    float ClothScatterBrightness;
    float FoliageRoughness;
    float GrassBrightness;
    float GrassBentNormal;
    float GrassRoughness;
    float WindIntensity;
    float AmbientDiffuse;
    float AmbientSpecular;
    float SSSAmount;
    float WaterRoughness;
    float WaterScatter;
    float WaterReflection;

    float4 SunColor;
    float4 SunDirection;
    float4 ClampSunDirection;
    bool Outdoor;
    float ViewRadius;
    float ViewHeight;
    float ZenithHorizonAngle;
    float SunBrightness;
    float RayleighScattering;
    float MieScattering;
    float MieAnisotropy;
    float MieHeight;
    float OzoneAbsorption;
    float FogDistance;
    float MinSunAngle;
    float SkyLightIntensity;
    float WorldScale;
    float AverageElevation;
    float RoughnessAdjust;
}

cbuffer PBRPerPass : register(b7)
{	
    bool PBRTexture;
    bool IsCloth;
    float Padding1;
    float Padding2;
}