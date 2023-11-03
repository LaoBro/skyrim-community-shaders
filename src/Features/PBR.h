#pragma once

#include "Buffer.h"
#include "Feature.h"

struct PBR : Feature
{
	static PBR* GetSingleton()
	{
		static PBR singleton;
		return &singleton;
	}

	static void InstallHooks()
	{
		Hooks::Install();
	}

	virtual inline std::string GetName() { return "PBR"; }
	virtual inline std::string GetShortName() { return "PBR"; }

	struct alignas(16) Settings
	{	
		std::uint32_t Outdoor = 1;
		std::uint32_t IndoorSunSpecular = 1;
		std::uint32_t EnableClothShader = 0;
		float ClothDiffuse =  0.5f;
		float ClothScatterDensity = 0.5f;
		float ClothScatterBrightness = 0.25f;
		float FoliageRoughness = 0.5f;
		float GrassBentNormal = 0.5f;
		float GrassRoughness = 0.66f;
		float GrassSpecular = 1.0f;
		float GrassDiffuse = 0.3f;
		float GrassSheen = 1.0f;
		float LandscapeSheen = 0.0f;
		float WindIntensity = 1.0f;
		float AmbientDiffuse = 1.0f;
		float AmbientSpecular = 1.0f;
		float SSSAmount = 1.0f;
		float WaterRoughness = 0.3f;
		float WaterAttenuation = 0.2f;
		float WaterReflection = 1.0f;
	};

	struct alignas(16) PerFrame
	{	
		DirectX::XMFLOAT4 EyePosition;
		Settings Settings;
	};

	Settings settings;

	bool updatePerFrame = false;
	bool ModelSpaceNormals = false;
	PerFrame perFrameData{};
	ConstantBuffer* perFrame = nullptr;
	ID3D11Buffer* perFramebuffers[1];

	struct alignas(16) PerPass
	{	
		std::uint32_t PBRTexture = 0;
		std::uint32_t IsCloth = 0;
		float Padding2 = 0.0f;
		float Padding3 = 0.0f;
	};
	PerPass perPassData{};
	ConstantBuffer* perPass = nullptr;
	ID3D11Buffer* perPassbuffers[1];

	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	void ModifyLighting(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	void BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* Pass);
	void BSLightingShader_SetupGeometry_After(RE::BSRenderPass* Pass);

	struct Hooks
	{
		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				func(This, Pass, RenderFlags);
				//GetSingleton()->BSLightingShader_SetupGeometry_After(Pass);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);

			logger::info("[PBR] Installed hooks");
		}
	};

};
