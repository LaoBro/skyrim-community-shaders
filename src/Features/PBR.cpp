#include "PBR.h"

#include "State.h"
#include "Util.h"

#include "Features/Clustered.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PBR::Settings,
	MinRoughness,
	MiddleRoughness,
	MaxRoughness,
	GlossinessScale,
	NonMetalThreshold,
	MetalThreshold,
	GrassBentNormal,
	GrassRoughness,
	GrassSpecular,
	GrassDiffuse,
	WindIntensity,
	WindScale,
	Exposure,
	PointLightAttenuation,
	PointLightIntensity,
	AmbientDiffuse,
	AmbientSpecular,
	SpecularToF0,
	CubemapToF0,
	DiffuseRimLighting,
	SpecularRimLighting,
	SoftLighting,
	WaterRoughness
	)

void PBR::DrawSettings()
{
	if (ImGui::TreeNodeEx("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::SliderFloat("Min Roughness", &settings.MinRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Middle Roughness", &settings.MiddleRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Max Roughness", &settings.MaxRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Glossiness Scale", &settings.GlossinessScale, 0.0f, 2.0f);
		ImGui::SliderFloat("NonMetal Threshold", &settings.NonMetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Metal Threshold", &settings.MetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass BentNormal", &settings.GrassBentNormal, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Roughness", &settings.GrassRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Specular", &settings.GrassSpecular, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Diffuse", &settings.GrassDiffuse, 0.0f, 1.0f);
		ImGui::SliderFloat("Wind Intensity", &settings.WindIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Wind Scale", &settings.WindScale, 0.0f, 1.0f);
		ImGui::SliderFloat("Exposure", &settings.Exposure, 0.0f, 2.0f);
		ImGui::SliderFloat("PointLight Attenuation", &settings.PointLightAttenuation, 0.0f, 2.0f);
		ImGui::SliderFloat("PointLight Intensity", &settings.PointLightIntensity, 0.0f, 5.0f);
		ImGui::SliderFloat("Ambient Diffuse", &settings.AmbientDiffuse, 0.0f, 2.0f);
		ImGui::SliderFloat("Ambient Specular", &settings.AmbientSpecular, 0.0f, 2.0f);
		ImGui::SliderFloat("Specular To F0", &settings.SpecularToF0, 0.0f, 2.0f);
		ImGui::SliderFloat("Cubemap To F0", &settings.CubemapToF0, 0.0f, 2.0f);
		ImGui::SliderFloat("Diffuse RimLighting", &settings.DiffuseRimLighting, 0.0f, 2.0f);
		ImGui::SliderFloat("Specular RimLighting", &settings.SpecularRimLighting, 0.0f, 2.0f);
		ImGui::SliderFloat("Soft Lighting", &settings.SoftLighting, 0.0f, 1.0f);
		ImGui::SliderFloat("Water Roughness", &settings.WaterRoughness, 0.0f, 0.5f);

		ImGui::TreePop();
	}

	ImGui::EndTabItem();
}

void PBR::ModifyLighting(const RE::BSShader*, const uint32_t)
{
	if (updatePerFrame) {
		PerFrame perFrameData{};
		ZeroMemory(&perFrameData, sizeof(perFrameData));

		perFrameData.Settings = settings;
		float a2 = settings.WaterRoughness;
		a2 *= a2;
		a2 *= a2;
		perFrameData.Settings.WaterRoughness = a2;

		auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
		auto& position = accumulator->GetRuntimeData().eyePosition;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

		RE::NiPoint3 eyePosition{};
		if (REL::Module::IsVR()) {
			// find center of eye position
			eyePosition = state->GetVRRuntimeData().posAdjust.getEye() + state->GetVRRuntimeData().posAdjust.getEye(1);
			eyePosition /= 2;
		} else
			eyePosition = state->GetRuntimeData().posAdjust.getEye();
		perFrameData.EyePosition.x = position.x - eyePosition.x;
		perFrameData.EyePosition.y = position.y - eyePosition.y;
		perFrameData.EyePosition.z = position.z - eyePosition.z;

    	if (auto sky = RE::Sky::GetSingleton())
        	perFrameData.Settings.outdoor = sky->mode.get() == RE::Sky::Mode::kFull;

		auto sunLight = skyrim_cast<RE::NiDirectionalLight*>(accumulator->GetRuntimeData().activeShadowSceneNode->GetRuntimeData().sunLight->light.get());
		if (sunLight) {
			auto& direction = sunLight->GetWorldDirection();
			perFrameData.DirLightDirection.x = direction.x;
			perFrameData.DirLightDirection.y = direction.y;
			perFrameData.DirLightDirection.z = direction.z;
		}

		perFrame->Update(perFrameData);
		updatePerFrame = false;
	}

	Clustered::GetSingleton()->Bind(true);
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	ID3D11Buffer* buffers[1];
	buffers[0] = perFrame->CB();
	context->VSSetConstantBuffers(6, 1, buffers);
	context->PSSetConstantBuffers(6, 1, buffers);
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
}

void PBR::Reset()
{
	updatePerFrame = true;
}