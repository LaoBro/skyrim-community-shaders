#include "Test.h"

#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	Test::Settings,
	Glossiness,
	SpecularStrength,
	SubsurfaceScatteringAmount,
	OverrideComplexGrassSettings,
	BasicGrassBrightness)

enum class GrassShaderTechniques
{
	RenderDepth = 8,
};

void Test::DrawSettings()
{
	if (ImGui::TreeNodeEx("Complex Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("Specular highlights for complex grass");
		ImGui::SliderFloat("Glossiness", &settings.Glossiness, 1.0f, 100.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Specular highlight glossiness.");
		}

		ImGui::SliderFloat("Specular Strength", &settings.SpecularStrength, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Specular highlight strength.");
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("SSS Amount", &settings.SubsurfaceScatteringAmount, 0.0f, 2.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"Subsurface Scattering (SSS) amount. "
				"Soft lighting controls how evenly lit an object is. "
				"Back lighting illuminates the back face of an object. "
				"Combined to model the transport of light through the surface. ");
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Override Complex Grass Lighting Settings", (bool*)&settings.OverrideComplexGrassSettings);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"Override the settings set by the grass mesh author. "
				"Complex grass authors can define the brightness for their grass meshes. "
				"However, some authors may not account for the extra lights available from Community Shaders. "
				"This option will treat their grass settings like non-complex grass. "
				"This was the default in Community Shaders < 0.7.0");
		}
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TextWrapped("Basic Grass");
		ImGui::SliderFloat("Brightness", &settings.BasicGrassBrightness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Darkens the grass textures to look better with the new lighting");
		}

		ImGui::TreePop();
	}
}

void Test::ModifyGrass(const RE::BSShader*, const uint32_t descriptor)
{
	const auto technique = descriptor & 0b1111;
	if (technique != static_cast<uint32_t>(GrassShaderTechniques::RenderDepth)) {
		if (updatePerFrame) {
			auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();

			PerFrame perFrameData{};
			perFrameData.SunlightScale = !REL::Module::IsVR() ?
			                                 imageSpaceManager->GetRuntimeData().data.baseData.hdr.sunlightScale :
			                                 imageSpaceManager->GetVRRuntimeData().data.baseData.hdr.sunlightScale;
			perFrameData.Settings = settings;
			perFrame->Update(perFrameData);

			updatePerFrame = false;
		}

		auto& context = State::GetSingleton()->context;

		ID3D11Buffer* buffers[2];
		context->VSGetConstantBuffers(2, 1, buffers);  // buffers[0]
		buffers[1] = perFrame->CB();
		context->VSSetConstantBuffers(2, ARRAYSIZE(buffers), buffers);
		context->PSSetConstantBuffers(3, ARRAYSIZE(buffers), buffers);
	}
}

void Test::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	switch (shader->shaderType.get()) {
	case RE::BSShader::Type::Grass:
		ModifyGrass(shader, descriptor);
		break;
	}
}

void Test::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void Test::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

void Test::RestoreDefaultSettings()
{
	settings = {};
}

void Test::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
}

void Test::Reset()
{
	updatePerFrame = true;
}