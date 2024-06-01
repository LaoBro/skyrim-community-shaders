#include "Atmosphere.h"
#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	Atmosphere::CPUData,
	Latitude,
	AverageElevation,
	MinSunAngle,
	BottomRadius,
	AtmosphereThickness,
	RayleighDensity,
	MieDensity,
	OzoneDensity,
	RayleighHeight,
	MieHeight,
	MieAnisotropy,
	OzoneCenter,
	OzoneWidth,
	CloudBrightness,
	CloudSaturation,
	DrawDistance,
	XYScale,
	ZScale,
	TotalBrightness,
	MoonBrightness,
	DateBias,
	FogHeight,
	FogDensity,
	UseAerialPerspectiveLut)

void Atmosphere::DrawSettings()
{
	ImGui::InputFloat("Bottom Radius", &settings.BottomRadius);
	ImGui::InputFloat("Atmosphere Thickness", &settings.AtmosphereThickness);
	ImGui::InputFloat("Rayleigh Density", &settings.RayleighDensity);
	ImGui::InputFloat("Rayleigh Height", &settings.RayleighHeight);
	ImGui::InputFloat("Mie Density", &settings.MieDensity);
	ImGui::InputFloat("Mie Height", &settings.MieHeight);
	ImGui::SliderFloat("Mie Anisotropy", &settings.MieAnisotropy, 0, 1);
	ImGui::InputFloat("Ozone Density", &settings.OzoneDensity);
	ImGui::InputFloat("Ozone Center", &settings.OzoneCenter);
	ImGui::InputFloat("Ozone Width", &settings.OzoneWidth);
	ImGui::InputFloat("Fog Density", &settings.FogDensity);
	ImGui::InputFloat("Fog Height", &settings.FogHeight);
	ImGui::InputFloat("Draw Distance", &settings.DrawDistance);
	ImGui::InputFloat("XY Scale", &settings.XYScale);
	ImGui::InputFloat("Z Scale", &settings.ZScale);
	ImGui::InputFloat("Total Brightness", &settings.TotalBrightness);
	ImGui::InputFloat("Moon Brightness", &settings.MoonBrightness);
	ImGui::ColorEdit3("Ground Albedo", settings.GroundAlbedo, ImGuiColorEditFlags_Float);

	if (ImGui::TreeNodeEx("Sun Phase")) {
		ImGui::SliderAngle("Latitude", &settings.Latitude, -90, 90);
		ImGui::InputFloat("Average Elevation", &settings.AverageElevation);
		ImGui::SliderFloat("Min SunAngle", &settings.MinSunAngle, 0, 1);
		ImGui::InputFloat("Date Bias", &settings.DateBias);
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Cloud")) {
		ImGui::InputFloat("Cloud Brightness", &settings.CloudBrightness);
		ImGui::InputFloat("Cloud Saturation", &settings.CloudSaturation);
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Debug")) {
		//ImGui::Checkbox("Use Aerial Perspective Lut", &settings.UseAerialPerspectiveLut);
		float SunTransmittanceColor[3] = { settings.SunTransmittance.x, settings.SunTransmittance.y, settings.SunTransmittance.z };
		ImGui::ColorEdit3("Sun Transmittance", SunTransmittanceColor, ImGuiColorEditFlags_Float);
		ImGui::InputFloat("View Height", &settings.ViewHeight);
		ImGui::Image(TransmittanceLutSRV, ImVec2(256, 64), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));  // Flip Y coordinates
		ImGui::Image(MultiScatteringLutSRV, ImVec2(32, 32), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
		ImGui::Image(SkyViewLutSRV, ImVec2(128, 128), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
		ImGui::TreePop();
	}
}

void SetGameSettingFloat(std::string a_name, std::string a_section, float a_value)
{
	auto ini = RE::INISettingCollection::GetSingleton();
	ini->GetSetting(std::format("{}:{}", a_name, a_section))->data.f = a_value;
}

void SetGamePrefSettingBool(std::string a_name, std::string a_section, int a_value)
{
	auto ini = RE::INIPrefSettingCollection::GetSingleton();
	ini->GetSetting(std::format("{}:{}", a_name, a_section))->data.b = a_value;
}

void Atmosphere::SetupResources()
{
	SetGameSettingFloat("fSunBaseSize", "Weather", 0);
	/*auto ini = RE::INISettingCollection::GetSingleton();
    for (auto& setting : ini->settings) {
        logger::info("{}", setting->GetName());
    }*/
	SetGamePrefSettingBool("bVolumetricLightingEnable", "Display", 0);
	SetGamePrefSettingBool("bEnableImprovedSnow", "Display", 0);

	TransmittanceLutCS = GetAtmosphereCS(L"Data\\Shaders\\TransmittanceLutCS.hlsl");
	MultiScatteringLutCS = GetAtmosphereCS(L"Data\\Shaders\\MultiScatteringLutCS.hlsl");
	SkyViewLutCS = GetAtmosphereCS(L"Data\\Shaders\\SkyViewLutCS.hlsl");
	AerialPerspectiveLutCS = GetAtmosphereCS(L"Data\\Shaders\\AerialPerspectiveLutCS.hlsl");

	//auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	//auto device = renderer->GetRuntimeData().forwarder;
	auto& device = State::GetSingleton()->device;

	DX::ThrowIfFailed(device->CreateSamplerState(&ComputeSamplerDesc, &ComputeSampler));

	D3D11_TEXTURE2D_DESC texDesc = DefaultTex2dDesc;
	texDesc.Width = 256;
	texDesc.Height = 64;
	texDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;

	device->CreateTexture2D(&texDesc, NULL, &TransmittanceLutTexture);

	texDesc.Width = 32;
	texDesc.Height = 32;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	device->CreateTexture2D(&texDesc, NULL, &MultiScatteringLutTexture);

	texDesc.Width = 128;
	texDesc.Height = 128;
	texDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
	device->CreateTexture2D(&texDesc, NULL, &SkyViewLutTexture);

	// AerialPerspectiveLut
	D3D11_TEXTURE3D_DESC tex3DDesc = DefaultTex3dDesc;
	tex3DDesc.Width = 32;
	tex3DDesc.Height = 32;
	tex3DDesc.Depth = 32;
	tex3DDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	device->CreateTexture3D(&tex3DDesc, NULL, &AerialPerspectiveLutTexture);

	device->CreateShaderResourceView(TransmittanceLutTexture, NULL, &TransmittanceLutSRV);
	device->CreateShaderResourceView(MultiScatteringLutTexture, NULL, &MultiScatteringLutSRV);
	device->CreateShaderResourceView(SkyViewLutTexture, NULL, &SkyViewLutSRV);
	device->CreateShaderResourceView(AerialPerspectiveLutTexture, NULL, &AerialPerspectiveLutSRV);

	device->CreateUnorderedAccessView(TransmittanceLutTexture, NULL, &TransmittanceLutUAV);
	device->CreateUnorderedAccessView(MultiScatteringLutTexture, NULL, &MultiScatteringLutUAV);
	device->CreateUnorderedAccessView(SkyViewLutTexture, NULL, &SkyViewLutUAV);
	device->CreateUnorderedAccessView(AerialPerspectiveLutTexture, NULL, &AerialPerspectiveLutUAV);
}

void Atmosphere::Reset()
{
}

void Atmosphere::ClearShaderCache()
{
	if (TransmittanceLutCS) {
		TransmittanceLutCS->Release();
		TransmittanceLutCS = GetAtmosphereCS(L"Data\\Shaders\\TransmittanceLutCS.hlsl");
		;
	}
	if (MultiScatteringLutCS) {
		MultiScatteringLutCS->Release();
		MultiScatteringLutCS = GetAtmosphereCS(L"Data\\Shaders\\MultiScatteringLutCS.hlsl");
	}
	if (SkyViewLutCS) {
		SkyViewLutCS->Release();
		SkyViewLutCS = GetAtmosphereCS(L"Data\\Shaders\\SkyViewLutCS.hlsl");
	}
	if (AerialPerspectiveLutCS) {
		AerialPerspectiveLutCS->Release();
		AerialPerspectiveLutCS = GetAtmosphereCS(L"Data\\Shaders\\AerialPerspectiveLutCS.hlsl");
	}
}

void Atmosphere::Draw(const RE::BSShader*, const uint32_t)
{
	/*
    auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
    if (REL::Module::IsVR()) {
        imageSpaceManager->GetVRRuntimeData().currentBaseData->hdr.eyeAdaptStrength = settings.eyeAdaptStrength;
        imageSpaceManager->GetVRRuntimeData().data.baseData.hdr.eyeAdaptStrength = settings.eyeAdaptStrength;
    }
    else {
        imageSpaceManager->GetRuntimeData().currentBaseData->hdr.eyeAdaptStrength = settings.eyeAdaptStrength;
        imageSpaceManager->GetRuntimeData().data.baseData.hdr.eyeAdaptStrength = settings.eyeAdaptStrength;
    }
    auto sky = RE::Sky::GetSingleton();
    if (sky) {
        auto Weather = sky->currentWeather;
        if (Weather) {
            auto imageSpaces = Weather->imageSpaces[RE::TESWeather::ColorTime::kDay];
            imageSpaces->data.hdr.eyeAdaptStrength = settings.eyeAdaptStrength;
        }
    }*/
}

void Atmosphere::RestoreDefaultSettings()
{
	settings = {};
}

void Atmosphere::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void Atmosphere::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

bool Atmosphere::HasShaderDefine(RE::BSShader::Type)
{
	return false;
}