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

	virtual inline std::string GetName() { return "PBR"; }
	virtual inline std::string GetShortName() { return "PBR"; }

	struct Settings
	{	
		std::uint32_t outdoor =  1;
		float MinRoughness = 0.2f;
		float MiddleRoughness = 0.6f;
		float MaxRoughness = 1.0f;
		float GlossinessScale = 1.0f;
		float NonMetalThreshold = 0.1f;
		float MetalThreshold = 0.2f;
		float GrassBentNormal = 0.2f;
		float GrassRoughness = 0.9f;
		float GrassSpecular = 0.5f;
		float GrassDiffuse = 0.3f;
		float WindIntensity = 0.15f;
		float WindSpeed = 0.15f;
		float Exposure = 1.0f;
		float PointLightAttenuation = 1.0f;
		float PointLightIntensity = 1.0f;
		float AmbientDiffuse = 1.0f;
		float AmbientSpecular = 1.0f;
		float SpecularToF0 = 0.5f;
		float CubemapToF0 = 1.0f;
		float DiffuseRimLighting = 1.0f;
		float SpecularRimLighting = 1.0f;
		float SoftLighting = 0.0f;
		float WaterRoughness = 0.3;
	};

	struct alignas(16) PerFrame
	{	
		DirectX::XMFLOAT4 EyePosition;
		DirectX::XMFLOAT4 DirLightDirection;
		Settings Settings;
	};

	Settings settings;

	bool updatePerFrame = false;
	ConstantBuffer* perFrame = nullptr;
	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	void ModifyLighting(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
};
