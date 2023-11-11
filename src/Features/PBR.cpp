#include "PBR.h"

#include "State.h"
#include "Util.h"

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
	GrassSpecular,
	GrassOcclusion,
	GrassSheen,
	WindIntensity,
	AmbientDiffuse,
	AmbientSpecular,
	SSSAmount,
	WaterRoughness,
	WaterAttenuation,
	WaterReflection
	)

void PBR::DrawSettings()
{
	if (ImGui::TreeNodeEx("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::Checkbox("Indoor Sun Specular", (bool*)&settings.IndoorSunSpecular);
		ImGui::Checkbox("Enable Cloth Shader", (bool*)&settings.EnableClothShader);
		ImGui::SliderFloat("Cloth Diffuse", &settings.ClothDiffuse, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth Scatter Density", &settings.ClothScatterDensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth Scatter Brightness", &settings.ClothScatterBrightness, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloth ClothRoughness", &settings.ClothRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Foliage Roughness", &settings.FoliageRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass BentNormal", &settings.GrassBentNormal, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Roughness", &settings.GrassRoughness, 0.0f, 2.0f);
		ImGui::SliderFloat("Grass Specular", &settings.GrassSpecular, 0.0f, 4.0f);
		ImGui::SliderFloat("Grass Occlusion", &settings.GrassOcclusion, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Sheen", &settings.GrassSheen, 0.0f, 1.0f);
		ImGui::SliderFloat("Wind Intensity", &settings.WindIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Ambient Diffuse", &settings.AmbientDiffuse, 0.0f, 2.0f);
		ImGui::SliderFloat("Ambient Specular", &settings.AmbientSpecular, 0.0f, 2.0f);
		ImGui::SliderFloat("SSS Amount", &settings.SSSAmount, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Roughness", &settings.WaterRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Attenuation", &settings.WaterAttenuation, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Reflection", &settings.WaterReflection, 0.0f, 1.0f);
		//ImGui::Checkbox("Update Texture Name", &UpdateTextureName);
		//ImGui::InputText("", TextureName, 256);

		ImGui::TreePop();
	}

	ImGui::EndTabItem();
}

void PBR::ModifyLighting(const RE::BSShader*, const uint32_t)
{	
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	if (updatePerFrame) {
		perFrameData.Settings = settings;

		auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
		auto& position = accumulator->GetRuntimeData().eyePosition;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

		RE::NiPoint3 eyePosition{};
		if (REL::Module::IsVR()) {
			// find center of eye position
			eyePosition = state->GetVRRuntimeData().posAdjust.getEye() + state->GetVRRuntimeData().posAdjust.getEye(1);
			eyePosition /= 2;
		} 
		else {
			eyePosition = state->GetRuntimeData().posAdjust.getEye();
		}
		perFrameData.EyePosition.x = position.x - eyePosition.x;
		perFrameData.EyePosition.y = position.y - eyePosition.y;
		perFrameData.EyePosition.z = position.z - eyePosition.z;

    	if (auto sky = RE::Sky::GetSingleton()) {
        	perFrameData.Settings.Outdoor = sky->mode.get() == RE::Sky::Mode::kFull;
		}

		perFrame->Update(perFrameData);

		context->VSSetConstantBuffers(6, 1, perFramebuffers);
		context->PSSetConstantBuffers(6, 1, perFramebuffers);
		context->PSSetConstantBuffers(7, 1, perPassbuffers);

		updatePerFrame = false;
	}
	/*
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

void PBR::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
	perFramebuffers[0] = perFrame->CB();
	perPass = new ConstantBuffer(ConstantBufferDesc<PerPass>());
	perPassbuffers[0] = perPass->CB();
}

void PBR::Reset()
{
	updatePerFrame = true;
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

	perPass->Update(perPassData);
}
/*
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