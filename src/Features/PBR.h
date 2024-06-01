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
	inline std::string_view GetShaderDefineName() override { return "PBR"; }

	bool HasShaderDefine(RE::BSShader::Type shaderType) override;

	struct alignas(16) Settings
	{	
		DirectX::XMFLOAT3X4 WorldDirectionalAmbient;
		std::uint32_t IndoorSunSpecular = 0;
		std::uint32_t EnableClothShader = 1;
		float ClothDiffuse = 1.0f;
		float ClothScatterDensity = 0.5f;
		float ClothScatterBrightness = 1.0f;
		float ClothRoughness = 0.9f;
		float FoliageRoughness = 0.7f;
		float GrassBrightness = 0.5f;
		float GrassBentNormal = 0.8f;
		float GrassRoughness = 1.1f;
		float WindIntensity = 1.0f;
		float AmbientDiffuse = 1.0f;
		float AmbientSpecular = 0.7f;
		float SSSAmount = 1.0f;
		float WaterRoughness = 0.3f;
		float WaterReflection = 1.0f;
	};

	struct alignas(16) PerFrame
	{	
		float4 EyePosition;
		float4 SunDirection;
		float4 PBRSunColor = {1.0f, 1.0f, 1.0f, 1.0f};
		Settings Settings;
	};

	Settings settings;

	bool UpdatedPerFrame = false;
	bool ModelSpaceNormals = false;
	PerFrame perFrameData{};
	ConstantBuffer* perFrame = nullptr;
	ID3D11Buffer* perFramebuffers[1];

	struct alignas(16) PerPass
	{	
		std::uint32_t RenderingCubemap = 1;
		std::uint32_t PBRTexture = 0;
		std::uint32_t IsCloth = 0;
		float Padding2 = 0.0f;
	};
	PerPass perPassData{};
	ConstantBuffer* perPass = nullptr;
	ID3D11Buffer* perPassbuffers[1];

	virtual void SetupResources();
	virtual void Reset();
	virtual void ClearShaderCache() override;

	virtual void DrawSettings();
	void ModifyLighting(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();

	bool SupportsVR() override { return false; };

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
	

	void SetGameSettingFloat(std::string a_name, std::string a_section, float a_value)
	{
		auto ini = RE::INISettingCollection::GetSingleton();
		ini->GetSetting(std::format("{}:{}", a_name, a_section))->data.f = a_value;
	}

	void ShowColor(float3 vec) {
		float vecarray[3] = {vec.x, vec.y, vec.z};
		ImGui::ColorEdit3("", vecarray);
	}

	void ShowMatrix(Matrix mat) {
		float VM1[4] = {mat._11, mat._12, mat._13, mat._14};
		float VM2[4] = {mat._21, mat._22, mat._23, mat._24};
		float VM3[4] = {mat._31, mat._32, mat._33, mat._34};
		float VM4[4] = {mat._41, mat._42, mat._43, mat._44};
		ImGui::InputFloat4("", VM1);
		ImGui::InputFloat4("", VM2);
		ImGui::InputFloat4("", VM3);
		ImGui::InputFloat4("", VM4);
	}

	Matrix GetInvInvViewProjMatrix() {
		Matrix ViewProjMatrix;

		auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
		if (shadowState->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS) {
			return ViewProjMatrix;
		}
		if (REL::Module::IsVR()) {
			ViewProjMatrix = shadowState->GetVRRuntimeData().cameraData.getEye().viewProjMatrixUnjittered;
		}
		else {
			ViewProjMatrix = shadowState->GetRuntimeData().cameraData.getEye().viewProjMatrixUnjittered;
		}

		return XMMatrixInverse(nullptr, ViewProjMatrix);
	}

};
