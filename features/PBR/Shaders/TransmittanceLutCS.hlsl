#include "Atmosphere.hlsl"

RWTexture2D<float4> TransmittanceLut : register(u0);

static const int SampleCount = 32;

float3 ComputeTransmittanceLut(float mu, float r) {
    float TriangleHeight2, StartWidth, EndWidth;
    IntersectCircleInside(r, mu, TriangleHeight2, StartWidth, EndWidth);
    float SampleLength = (EndWidth - StartWidth) / float(SampleCount);
	float TriangleWidth = StartWidth + SampleLength * 0.5;
    float3 TotalDensity = 0.0;
	for (int i = 0; i < SampleCount; i++) {
        float SampleRadius = sqrt(TriangleHeight2 + TriangleWidth * TriangleWidth);
        float SampleHeight = SampleRadius - BottomRadius;
        TotalDensity += GetDensity(SampleHeight);
        TriangleWidth += SampleLength;
    }
    float3 TotalExtinction = Density2Extinction(TotalDensity);
    return Extinction2Transmittance(TotalExtinction, SampleLength);
}

[numthreads(1, 64, 1)] 
void main(uint3 GlobalID : SV_DispatchThreadID)
{
    float mu, r;
    ID2muR(GlobalID.xy, mu, r, TransmittanceLutResolution);
    TransmittanceLut[GlobalID.xy] = float4(ComputeTransmittanceLut(mu, r), 1.0);
}