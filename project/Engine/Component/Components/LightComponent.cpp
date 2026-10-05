#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "externals/imgui/imgui.h"
#include <cmath>
#include <algorithm>

namespace {
    // 角度変換定数
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;

    // デフォルト値定数 (マジックナンバー排除)
    constexpr float kDefaultIntensity = 1.0f;
    constexpr float kDefaultRange = 10.0f;
    constexpr float kDefaultDecay = 1.0f;
    constexpr float kDefaultSpotAngle = 45.0f;
    constexpr float kDefaultSpotFalloffStart = 30.0f;

    // UI 制限・操作定数
    constexpr float kMinIntensity = 0.0f;
    constexpr float kMaxIntensity = 100.0f;
    constexpr float kMinRange = 0.1f;
    constexpr float kMaxRange = 1000.0f;
    constexpr float kMinDecay = 0.0f;
    constexpr float kMaxDecay = 10.0f;
    constexpr float kMinSpotAngle = 0.1f;
    constexpr float kMaxSpotAngle = 89.9f;

    constexpr float kDragSpeedIntensity = 0.05f;
    constexpr float kDragSpeedRange = 0.2f;
    constexpr float kDragSpeedDecay = 0.02f;
    constexpr float kDragSpeedAngle = 0.5f;

    // 初期基準方向 (真下)
    const Vector3 kDefaultBaseDirection = { 0.0f, -1.0f, 0.0f };
}

LightComponent::LightComponent()
    : lightType_(LightType::Directional),
      color_{ 1.0f, 1.0f, 1.0f, 1.0f },
      intensity_(kDefaultIntensity),
      range_(kDefaultRange),
      decay_(kDefaultDecay),
      spotAngle_(kDefaultSpotAngle),
      spotFalloffStart_(kDefaultSpotFalloffStart),
      isRegistered_(false) {
}

LightComponent::~LightComponent() {
    UnregisterFromLightManager();
}

void LightComponent::Initialize() {
    RegisterToLightManager();
}

void LightComponent::Update() {
    if (!isRegistered_) {
        RegisterToLightManager();
    }
}

Vector3 LightComponent::GetDirection() const {
    if (!owner_) {
        return kDefaultBaseDirection;
    }

    const Vector3& rot = owner_->GetRotate();
    Matrix4x4 rotMat = Math::MakeRotateMatrix(rot.x, rot.y, rot.z);
    Vector3 dir = Math::TransformNormal(kDefaultBaseDirection, rotMat);
    return Math::Normalize(dir);
}

DirectionalLight LightComponent::GetDirectionalLightData() const {
    DirectionalLight data{};
    data.color = color_;
    data.direction = GetDirection();
    data.intensity = intensity_;
    return data;
}

PointLight LightComponent::GetPointLightData() const {
    PointLight data{};
    data.color = color_;
    data.position = owner_ ? owner_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    data.intensity = intensity_;
    data.radius = range_;
    data.decay = decay_;
    return data;
}

SpotLight LightComponent::GetSpotLightData() const {
    SpotLight data{};
    data.color = color_;
    data.position = owner_ ? owner_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    data.direction = GetDirection();
    data.intensity = intensity_;
    data.distance = range_;
    data.decay = decay_;

    float radAngle = spotAngle_ * kDegToRad;
    float radFalloff = spotFalloffStart_ * kDegToRad;
    data.cosAngle = std::cos(radAngle);
    data.cosFalloffStart = std::cos(radFalloff);
    return data;
}

void LightComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string tag = "##Light_" + (owner_ ? owner_->GetName() : "Unnamed");

    if (ImGui::CollapsingHeader(("Light" + tag).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        // タイプ選択
        const char* lightTypeNames[] = { "Directional (平行光)", "Point (点光源)", "Spot (スポットライト)" };
        int currentType = static_cast<int>(lightType_);
        if (ImGui::Combo(("Type" + tag).c_str(), &currentType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames))) {
            lightType_ = static_cast<LightType>(currentType);
        }

        // カラーピッカー
        ImGui::ColorEdit4(("Color" + tag).c_str(), &color_.x);

        // 輝度
        ImGui::DragFloat(("Intensity" + tag).c_str(), &intensity_, kDragSpeedIntensity, kMinIntensity, kMaxIntensity, "%.2f");

        // タイプ別パラメータ
        if (lightType_ == LightType::Point || lightType_ == LightType::Spot) {
            const char* rangeLabel = (lightType_ == LightType::Point) ? "Radius" : "Distance";
            std::string rangeStr = std::string(rangeLabel) + tag;
            ImGui::DragFloat(rangeStr.c_str(), &range_, kDragSpeedRange, kMinRange, kMaxRange, "%.1f");

            ImGui::DragFloat(("Decay" + tag).c_str(), &decay_, kDragSpeedDecay, kMinDecay, kMaxDecay, "%.2f");
        }

        if (lightType_ == LightType::Spot) {
            if (ImGui::SliderFloat(("Spot Angle" + tag).c_str(), &spotAngle_, kMinSpotAngle, kMaxSpotAngle, "%.1f deg")) {
                if (spotAngle_ < spotFalloffStart_) {
                    spotFalloffStart_ = spotAngle_;
                }
            }
            if (ImGui::SliderFloat(("Falloff Start" + tag).c_str(), &spotFalloffStart_, kMinSpotAngle, spotAngle_, "%.1f deg")) {
                if (spotFalloffStart_ > spotAngle_) {
                    spotAngle_ = spotFalloffStart_;
                }
            }
        }
    }
#endif
}

void LightComponent::RegisterToLightManager() {
    if (auto lightMgr = LightResource::GetLightManager()) {
        lightMgr->RegisterLightComponent(this);
        isRegistered_ = true;
    }
}

void LightComponent::UnregisterFromLightManager() {
    if (auto lightMgr = LightResource::GetLightManager()) {
        lightMgr->UnregisterLightComponent(this);
    }
    isRegistered_ = false;
}
