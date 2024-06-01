#pragma once

#include "Buffer.h"
#include "Feature.h"

struct Test : Feature
{
	static Test* GetSingleton()
	{
		static Test singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "Test"; }
	virtual inline std::string GetShortName() { return "Test"; }

	struct Settings
	{
		float Glossiness = 20.0f;
		float SpecularStrength = 0.5f;
		float SubsurfaceScatteringAmount = 1.0f;
		uint OverrideComplexGrassSettings = false;
		float BasicGrassBrightness = 1.0f;
	};

	struct alignas(16) PerFrame
	{
		float SunlightScale;
		Settings Settings;
		float pad[2];
	};

	Settings settings;

	bool updatePerFrame = false;
	ConstantBuffer* perFrame = nullptr;
	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	void ModifyGrass(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();

	bool SupportsVR() override { return true; };
};
