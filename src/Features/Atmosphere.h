#pragma once
#include "Util.h"
#include "Feature.h"
using namespace std;
static const float PI = 3.1415926535;

struct Atmosphere : Feature
{

	static Atmosphere* GetSingleton()
	{
		static Atmosphere singleton;
		return &singleton;
	}

	static void InstallHooks()
	{

	}

	virtual inline std::string GetName() { return "Atmosphere"; }
	virtual inline std::string GetShortName() { return "Atmosphere"; }

	virtual void SetupResources();
	virtual void Reset();
	virtual void ClearShaderCache() override;

	virtual void DrawSettings();
	virtual void Draw(const RE::BSShader*, const uint32_t);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();

	#pragma region // Setting and PerFrame data

	struct alignas(16) AtmospherePerFrame
	{	
		float3 SunColor;
		uint Outdoor = 1;

		float3 SunDirection;
		uint UseAerialPerspectiveLut;
		
		float3 ClampSunDirection;
		float ViewHeight;

		float ViewRadius;
		float ViewRadius2;
		float BottomRadius;
		float BottomRadius2;

		float AtmosphereThickness;
		float TopRadius;
		float TopRadius2;
		float RayleighDensity;

		float MieDensity;
		float OzoneDensity;
		float RayleighHeight;
		float InvRayleighHeight;

		float MieHeight;
		float InvMieHeight;
		float MieAnisotropy;
		float OzoneCenter;

		float InvOzoneWidth;
		float3 GroundAlbedo;

		float ZenithHorizonAngle;
		float Beta;
		float H;
		float TotalBrightness;

		float MoonBrightness;
		float DrawDistance;
		float WorldScale;
		float CloudBrightness;

		float CloudSaturation;
		float FogHeight;
		float InvFogHeight;
		float FogDensity;

	};

	AtmospherePerFrame PerFrameData;

	struct CPUData {
		//CPU only
		float Latitude = 0.5;
		float AverageElevation = 0.4;
		float MinSunAngle = 0.0;
		float DateBias = 284;
		float3 SunTransmittance;
		bool UpdatedPerFrame = false;
		bool NeedUpdateLut = true;
		//CPU and GPU
		float TotalBrightness = 12;
		float MoonBrightness = 0.5;
		float DrawDistance = 6;
		float XYScale = 1;
		float ZScale = 1;
		float CloudBrightness = 2;
		float CloudSaturation = 0;
		float MieAnisotropy = 0.8;
		float3 SunColor;
		float3 SunDirection;
		float3 ClampSunDirection;
		bool Outdoor = 1;
		bool UseAerialPerspectiveLut = 1;
		//About the Lut
		float BottomRadius = 6360;
		float AtmosphereThickness = 100;
		float RayleighDensity  = 1;
		float MieDensity = 2;
		float OzoneDensity = 2;
		float FogDensity = 5;
		float RayleighHeight = 8;
		float MieHeight = 1.2;
		float OzoneCenter = 25;
		float OzoneWidth = 25;
		float FogHeight = 4;
		float GroundAlbedo[3] = {0.3, 0.3, 0.3};
		//GPU only
		float BottomRadius2;
		float TopRadius;
		float TopRadius2;
		float ViewHeight;
		float ViewRadius;
		float ViewRadius2;
		float InvOzoneWidth;

		void UpdateSunDirection() {
			if (auto calendar = RE::Calendar::GetSingleton()) {
				float time = calendar->GetCurrentGameTime();
				float Beta = PI * (0.5f + 0.13027f * sin(2 * PI * (DateBias + time) / 365));
				//take 12:00 as the origin
				float Phi = PI * 2 * (time - 0.5f);

				float SinPhi = sin(Phi);
				float CosPhi = cos(Phi);
				float SinLatitude = sin(Latitude);
				float CosLatitude = cos(Latitude);

				float3 SunCenter = {0, CosLatitude, SinLatitude};
				float SunCenterDistance = cos(Beta);
				SunCenter *= SunCenterDistance;

				SunDirection = float3(
					SinPhi,
					CosPhi * SinLatitude,
					CosPhi * CosLatitude
				);
				float SunRadius = sin(Beta);
				SunDirection *= SunRadius;
				SunDirection += SunCenter;
				SunDirection.Normalize();
			}
			else {
				SunDirection = float3(1, 0, 0);
			}
		}

		float4 GetDensity(float height) {
		return {
			exp(-height / RayleighHeight) * RayleighDensity,
			exp(-height / MieHeight) * MieDensity,
			exp(-height / FogHeight) * FogDensity,
			(1.0f - min(abs(height - OzoneCenter) * InvOzoneWidth, 1.0f)) * OzoneDensity
			} ;
		}

		float3 Density2Extinction(float4 Density) {
			return Density.x * float3(5.8e-3f, 13.5e-3f, 33.1e-3f) + 
				Density.y * float3(3.996e-3f + 4.440e-3f) + 
				Density.z * float3(3.996e-3f) + 
				Density.w * float3(0.650e-3f, 1.881e-3f, 0.085e-3f);
		}

		float3 Extinction2Transmittance(float3 Extinction, float SegmentLength) {
			Extinction *= -SegmentLength;
			return float3(exp(Extinction.x), exp(Extinction.y), exp(Extinction.z));
		}

		float3 GetSunTransmittance() {
			float StartWidth = ViewRadius * SunDirection.z;
			float TriangleHeight2 = ViewRadius2 - StartWidth * StartWidth;
			float EndWidth = sqrt(TopRadius2 - TriangleHeight2);

			static const int SampleCount = 32;
			float SampleLength = (EndWidth - StartWidth) / float(SampleCount);
			float TriangleWidth = StartWidth + SampleLength * 0.5f;
			float4 TotalDensity = float4(0,0,0,0);

			for (int i = 0; i < SampleCount; i++) {
				float SampleRadius = sqrt(TriangleHeight2 + TriangleWidth * TriangleWidth);
				float SampleHeight = SampleRadius - BottomRadius;
				TotalDensity += GetDensity(SampleHeight);
				TriangleWidth += SampleLength;
			}
			float3 TotalExtinction = Density2Extinction(TotalDensity);
			float3 Transmittance = Extinction2Transmittance(TotalExtinction, SampleLength);
			return Transmittance;
		}

		void ToGPUData(AtmospherePerFrame *PerFrameData1) {

			Outdoor = false;
			auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
			auto SunLight = skyrim_cast<RE::NiDirectionalLight*>(accumulator->GetRuntimeData().activeShadowSceneNode->GetRuntimeData().sunLight->light.get());
			auto sky = RE::Sky::GetSingleton();
			if (SunLight && sky) {
				Outdoor = sky->mode.get() == RE::Sky::Mode::kFull;
			}
			PerFrameData1->Outdoor = Outdoor;
			if (!Outdoor) {
				return;
			}

			auto Position = accumulator->GetRuntimeData().eyePosition;
			ViewHeight = Position.z * ZScale * 1.428e-5f + AverageElevation;
			ViewHeight = max(ViewHeight, 0.0f);

			PerFrameData1->BottomRadius = BottomRadius;
			BottomRadius2 = BottomRadius * BottomRadius;
			PerFrameData1->BottomRadius2 = BottomRadius2;
			PerFrameData1->AtmosphereThickness = AtmosphereThickness;
			TopRadius = BottomRadius + AtmosphereThickness;
			PerFrameData1->TopRadius = TopRadius;
			TopRadius2 = TopRadius * TopRadius;
			PerFrameData1->TopRadius2 = TopRadius2;
			PerFrameData1->RayleighDensity = RayleighDensity;
			PerFrameData1->MieDensity = MieDensity;
			PerFrameData1->OzoneDensity = OzoneDensity;
			PerFrameData1->FogDensity = FogDensity;
			PerFrameData1->RayleighHeight = RayleighHeight;
			PerFrameData1->InvRayleighHeight = 1 / max(RayleighHeight, 1e-4f);
			PerFrameData1->MieHeight = MieHeight;
			PerFrameData1->InvMieHeight = 1 / max(MieHeight, 1e-4f);
			PerFrameData1->OzoneCenter = OzoneCenter;
			InvOzoneWidth = 1 / max(OzoneWidth, 1e-4f);
			PerFrameData1->InvOzoneWidth = InvOzoneWidth;
			PerFrameData1->FogHeight = FogHeight;
			PerFrameData1->InvFogHeight = 1 / max(FogHeight, 1e-4f);
			PerFrameData1->MieAnisotropy = MieAnisotropy;
			PerFrameData1->GroundAlbedo = float3(GroundAlbedo);

			PerFrameData1->ViewHeight = ViewHeight;;
			ViewRadius = ViewHeight + BottomRadius;
			PerFrameData1->ViewRadius = ViewRadius;
			ViewRadius2 = ViewRadius * ViewRadius;
			PerFrameData1->ViewRadius2 = ViewRadius2;
			
			float Vhorizon = sqrt(ViewRadius * ViewRadius - BottomRadius2);
			float CosBeta = Vhorizon / ViewRadius;				// GroundToHorizonCos
			float Beta = acos(CosBeta);
			PerFrameData1->Beta = Beta;
			PerFrameData1->ZenithHorizonAngle = PI - Beta;
			float H = sqrt(TopRadius2 - BottomRadius2);
			PerFrameData1->H = H;

			PerFrameData1->TotalBrightness = TotalBrightness;
			PerFrameData1->MoonBrightness = MoonBrightness;
			PerFrameData1->DrawDistance = DrawDistance;
			PerFrameData1->WorldScale = XYScale * 1.428e-5f;
			PerFrameData1->CloudBrightness = CloudBrightness;
			PerFrameData1->CloudSaturation = CloudSaturation;

			UpdateSunDirection();
			ClampSunDirection = SunDirection;
			ClampSunDirection.z = abs(ClampSunDirection.z);
			ClampSunDirection.z = max(ClampSunDirection.z, MinSunAngle);
			ClampSunDirection.Normalize();
			PerFrameData1->SunDirection = SunDirection;
			PerFrameData1->ClampSunDirection = ClampSunDirection;
			SunLight->world.rotate.entry[0][0] = -ClampSunDirection.x;
			SunLight->world.rotate.entry[1][0] = -ClampSunDirection.y;
			SunLight->world.rotate.entry[2][0] = -ClampSunDirection.z;
			SunLight->GetDirectionalLightRuntimeData().worldDir = RE::NiPoint3(-ClampSunDirection.x, -ClampSunDirection.y, -ClampSunDirection.z);

			auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
			if (REL::Module::IsVR()) {
				imageSpaceManager->GetVRRuntimeData().data.baseData.hdr.sunlightScale = 1;
			}
			else {
				imageSpaceManager->GetRuntimeData().data.baseData.hdr.sunlightScale = 1;
			}

			SunTransmittance = GetSunTransmittance();
			SunColor = float3(1.0, 0.949, 0.937) * SunTransmittance * TotalBrightness / PI;
			SunColor += float3(0.1);
			SunColor = SunColor * max(MoonBrightness / SunColor.x, 1.0f);
			PerFrameData1->SunColor = SunColor;

			SunLight->GetLightRuntimeData().diffuse.red = SunColor.x;
			SunLight->GetLightRuntimeData().diffuse.green = SunColor.y;
			SunLight->GetLightRuntimeData().diffuse.blue = SunColor.z;

			PerFrameData1->UseAerialPerspectiveLut = UseAerialPerspectiveLut;

			/*auto Weather = sky->currentWeather;
			if (Weather) {
				for (int i = 0; i < 4; i++) {
					auto volumetricLighting = Weather->volumetricLighting[i];
					if (volumetricLighting) {
						volumetricLighting->intensity = 0;
					}
				}
			}*/

		}
	};

	CPUData settings;

	#pragma endregion

	#pragma region // Sun direction & transmittance

	#pragma endregion

	#pragma region // DX11 resource

	D3D11_TEXTURE2D_DESC DefaultTex2dDesc = {
		.Width = 32,
		.Height = 32,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
		.CPUAccessFlags = 0,
		.MiscFlags = 0
	};

	D3D11_TEXTURE3D_DESC DefaultTex3dDesc = {
		.Width = 32,
		.Height = 32,
		.Depth = 32,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
		.CPUAccessFlags = 0,
		.MiscFlags = 0
	};

	D3D11_SAMPLER_DESC ComputeSamplerDesc = {
		.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.MaxAnisotropy = 1,
		.MinLOD = 0,
		.MaxLOD = D3D11_FLOAT32_MAX
	};

	ID3D11SamplerState* ComputeSampler = nullptr;

	ID3D11Texture2D* TransmittanceLutTexture = nullptr;
	ID3D11Texture2D* MultiScatteringLutTexture = nullptr;
	ID3D11Texture2D* SkyViewLutTexture = nullptr;
	ID3D11Texture3D* AerialPerspectiveLutTexture = nullptr;

	ID3D11ShaderResourceView* TransmittanceLutSRV = nullptr;
	ID3D11ShaderResourceView* MultiScatteringLutSRV = nullptr;
	ID3D11ShaderResourceView* SkyViewLutSRV = nullptr;
	ID3D11ShaderResourceView* AerialPerspectiveLutSRV = nullptr;
	ID3D11ShaderResourceView* NullSRV = nullptr;

	ID3D11UnorderedAccessView* TransmittanceLutUAV = nullptr;
	ID3D11UnorderedAccessView* MultiScatteringLutUAV = nullptr;
	ID3D11UnorderedAccessView* SkyViewLutUAV = nullptr;
	ID3D11UnorderedAccessView* AerialPerspectiveLutUAV = nullptr;
	ID3D11UnorderedAccessView* NullUAV = nullptr;

	ID3D11ComputeShader* TransmittanceLutCS = nullptr;
	ID3D11ComputeShader* MultiScatteringLutCS = nullptr;
	ID3D11ComputeShader* SkyViewLutCS = nullptr;
	ID3D11ComputeShader* AerialPerspectiveLutCS = nullptr;
	#pragma endregion

	ID3D11ComputeShader* GetAtmosphereCS(const wchar_t Path[]) {
		return (ID3D11ComputeShader*)Util::CompileShader(Path, {}, "cs_5_0");
	}

	float4 ToFloat4(float3 Value) {
		return {Value.x, Value.y, Value.z, 0};
	}

	void UpdatePerFrame() {
		settings.ToGPUData(&PerFrameData);
	}

	void ComputePerFrame() {

		if (!settings.Outdoor) {
			return;
		}

		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
		
		if (settings.NeedUpdateLut) {
			//TransmittanceLut
			context->CSSetSamplers(15, 1, &ComputeSampler);
			context->CSSetShader(TransmittanceLutCS, nullptr, 0);
			context->CSSetUnorderedAccessViews(0, 1, &TransmittanceLutUAV, nullptr);
			context->Dispatch(256, 1, 1);
			//MultiScatteringLut
			context->CSSetShader(MultiScatteringLutCS, nullptr, 0);
			context->CSSetUnorderedAccessViews(0, 1, &MultiScatteringLutUAV, nullptr);
			context->CSSetShaderResources(0, 1, &TransmittanceLutSRV);
			context->Dispatch(32, 32, 1);
		}
		//AerialPerspectiveLut
		context->CSSetShader(AerialPerspectiveLutCS, nullptr, 0);
		context->CSSetShaderResources(0, 1, &TransmittanceLutSRV);
		context->CSSetShaderResources(1, 1, &MultiScatteringLutSRV);
		context->CSSetUnorderedAccessViews(0, 1, &AerialPerspectiveLutUAV, nullptr);
		context->Dispatch(1, 16, 32);
		//SkyViewLut
		context->CSSetShader(SkyViewLutCS, nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, &SkyViewLutUAV, nullptr);
		context->CSSetShaderResources(2, 1, &AerialPerspectiveLutSRV);
		context->Dispatch(128, 2, 1);
		
		context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
		context->CSSetShaderResources(0, 1, &NullSRV);
		context->CSSetShaderResources(1, 1, &NullSRV);
		context->PSSetShaderResources(41, 1, &SkyViewLutSRV);
		context->PSSetShaderResources(42, 1, &AerialPerspectiveLutSRV);
		context->VSSetShaderResources(42, 1, &AerialPerspectiveLutSRV);
		//context->PSSetShaderResources(43, 1, &TransmittanceLutSRV);
		context->PSSetShaderResources(43, 1, &MultiScatteringLutSRV);
		context->VSSetSamplers(0, 1, &ComputeSampler);

	}

};
