#include "PBR.h"

#include "State.h"
#include "Util.h"
#include "Atmosphere.h"
#include "GrassLighting.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PBR::Settings,
	IndoorSunSpecular,
	EnableClothShader,
	ClothDiffuse,
	ClothScatterDensity,
	ClothScatterBrightness,
	ClothRoughness,
	FoliageRoughness,
	GrassBentNormal,
	GrassRoughness,
	WindIntensity,
	AmbientDiffuse,
	AmbientSpecular,
	SSSAmount,
	WaterRoughness,
	WaterReflection
	)

void PBR::DrawSettings()
{
	if (
		ImGui::TreeNodeEx("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Indoor Sun Specular", (bool*)&settings.IndoorSunSpecular);
		ImGui::Checkbox("Enable Cloth Shader", (bool*)&settings.EnableClothShader);
		ImGui::SliderFloat("Cloth Diffuse", &settings.ClothDiffuse, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth Scatter Density", &settings.ClothScatterDensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth Scatter Brightness", &settings.ClothScatterBrightness, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth Roughness", &settings.ClothRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Foliage Roughness", &settings.FoliageRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Brightness", &settings.GrassBrightness, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass BentNormal", &settings.GrassBentNormal, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Roughness", &settings.GrassRoughness, 0.0f, 2.0f);
		ImGui::SliderFloat("Wind Intensity", &settings.WindIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Ambient Diffuse", &settings.AmbientDiffuse, 0.0f, 2.0f);
		ImGui::SliderFloat("Ambient Specular", &settings.AmbientSpecular, 0.0f, 2.0f);
		ImGui::SliderFloat("SSS Amount", &settings.SSSAmount, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Roughness", &settings.WaterRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Reflection", &settings.WaterReflection, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	ImGui::EndTabItem();
}

struct alignas(16) MergedPerFrame {
	PBR::Settings Settings;
	Atmosphere::AtmospherePerFrame AtmospherePerFrameData;
};

MergedPerFrame MergedPerFrameData;
auto AtmosphereSingleton = Atmosphere::GetSingleton();

void PBR::ModifyLighting(const RE::BSShader*, const uint32_t)
{	
	if (UpdatedPerFrame) {
		return;
	}
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	//auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
	auto& state = RE::BSShaderManager::State::GetSingleton();
	RE::NiTransform& dalcTransform = state.directionalAmbientTransform;
	Util::StoreTransform3x4NoScale(settings.WorldDirectionalAmbient, dalcTransform);
	AtmosphereSingleton->UpdatePerFrame();
	
	MergedPerFrameData.AtmospherePerFrameData = AtmosphereSingleton->PerFrameData;
	MergedPerFrameData.Settings = settings;

	perFrame->Update(MergedPerFrameData);

	context->VSSetConstantBuffers(6, 1, perFramebuffers);
	context->PSSetConstantBuffers(6, 1, perFramebuffers);
	context->CSSetConstantBuffers(6, 1, perFramebuffers);
	context->PSSetConstantBuffers(7, 1, perPassbuffers);

	AtmosphereSingleton->ComputePerFrame();

	UpdatedPerFrame = true;

	/*  Get normal map in VS
	if (ModelSpaceNormals) {
		ID3D11ShaderResourceView* NormalView[1];
		context->PSGetShaderResources(1, 1, NormalView);
		context->VSSetShaderResources(1, 1, NormalView);

		ID3D11SamplerState* NormalSampler[1];
		context->PSGetSamplers(0, 1, NormalSampler);
		context->VSSetSamplers(0, 1, NormalSampler);
	}
	*/
	
}

void PBR::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	switch (shader->shaderType.get()) {
	case RE::BSShader::Type::Lighting:
		ModifyLighting(shader, descriptor);
		break;
	}
	//ModifyLighting(shader, descriptor);
}

void PBR::ClearShaderCache()
{
}

void PBR::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void PBR::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

void PBR::RestoreDefaultSettings()
{
	settings = {};
}

void PBR::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<MergedPerFrame>());
	perFramebuffers[0] = perFrame->CB();
	perPass = new ConstantBuffer(ConstantBufferDesc<PerPass>(true));
	perPassbuffers[0] = perPass->CB();
}

void PBR::Reset()
{
	UpdatedPerFrame = false;
	perPassData.RenderingCubemap = 1;
}

void PBR::BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* Pass)
{
	perPassData.PBRTexture = false;

	if (auto Geometry = Pass->geometry) {
		if (auto ExtraData = Geometry->GetExtraData("PBR")) {
			perPassData.PBRTexture = true;
		}
	}

	if (auto ShaderProperty = Pass->shaderProperty) {
		//ModelSpaceNormals = shaderProperty -> flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);
		if (ShaderProperty->GetMaterialType() == RE::BSShaderMaterial::Type::kLighting) {

			if (auto ExtraData = ShaderProperty->GetExtraData("PBR")) {
				perPassData.PBRTexture = true;
			}
			

			if (auto lightingMaterial = (RE::BSLightingShaderMaterialBase*)(ShaderProperty->material)) {
				if (auto TextureSet = lightingMaterial->GetTextureSet())
				{
					if (auto TexturePathChar = TextureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse)) {
						std::string TexturePathString = TexturePathChar;
						perPassData.IsCloth = TexturePathString.contains("clothes") && settings.EnableClothShader;
					}
				}
			}
		}
	}

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	perPassData.RenderingCubemap = shadowState->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS;

	perPass->Update(perPassData);
}

/* Get normal map in VS
void PBR::BSLightingShader_SetupGeometry_After(RE::BSRenderPass*)
{	
	if (ModelSpaceNormals) {
		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
		ID3D11ShaderResourceView* NormalView[1];
		context->PSGetShaderResources(1, 1, NormalView);
		context->VSSetShaderResources(1, 1, NormalView);

		ID3D11SamplerState* NormalSampler[1];
		context->PSGetSamplers(0, 1, NormalSampler);
		context->VSSetSamplers(0, 1, NormalSampler);
	}
}
*/