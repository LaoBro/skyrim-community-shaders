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

	struct Settings
	{	
		std::uint32_t Outdoor =  1;
		std::uint32_t Cloth =  0;
		float MinRoughness = 0.1f;
		float MiddleRoughness = 0.5f;
		float ClothDiffuse =  0.0f;
		float ClothScatter = 1.0f;
		float GrassBentNormal = 0.5f;
		float GrassRoughness = 0.9f;
		float GrassSpecular = 1.0f;
		float GrassDiffuse = 0.3f;
		float GrassSheen = 1.0f;
		float LandscapeSheen = 0.0f;
		float WindIntensity = 0.15f;
		float AmbientDiffuse = 1.0f;
		float AmbientSpecular = 1.0f;
		float SSSAmount = 1.0f;
		float SSSBlur = 1.0f;
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

	bool ModelSpaceNormals = false;
	bool UpdateTextureName = false;
	bool EnableClothShader = false;
	char TextureName[256] = "1234567890";

	bool updatePerFrame = false;
	ConstantBuffer* perFrame = nullptr;
	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	void ModifyLighting(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	void BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* Pass);

	struct Hooks
	{
		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				func(This, Pass, RenderFlags);
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
