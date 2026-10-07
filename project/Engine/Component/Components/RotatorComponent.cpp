#include "Engine/Component/Components/RotatorComponent.h"
#include "Engine/Component/GameObject.h"
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // デルタタイム定数（60FPS基準: マジックナンバー排除）
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    // インスペクター編集用定数
    constexpr float kDragSpeed = 0.01f;
    constexpr float kRadToDeg = 180.0f / 3.1415926535f;
    constexpr float kDegToRad = 3.1415926535f / 180.0f;
    constexpr float kTwoPi = 6.283185307f;
}

void RotatorComponent::Update() {
    if (!owner_ || !isActive_) return;

    Vector3 currentRot = owner_->GetRotate();
    Vector3 deltaRot = {
        rotationSpeed_.x * kDefaultDeltaTime,
        rotationSpeed_.y * kDefaultDeltaTime,
        rotationSpeed_.z * kDefaultDeltaTime
    };

    currentRot.x += deltaRot.x;
    currentRot.y += deltaRot.y;
    currentRot.z += deltaRot.z;

    // 角度のラップ処理（-2π〜2πに収めて数値膨張を抑制）
    if (std::abs(currentRot.x) > kTwoPi) currentRot.x = std::fmod(currentRot.x, kTwoPi);
    if (std::abs(currentRot.y) > kTwoPi) currentRot.y = std::fmod(currentRot.y, kTwoPi);
    if (std::abs(currentRot.z) > kTwoPi) currentRot.z = std::fmod(currentRot.z, kTwoPi);

    owner_->SetRotate(currentRot);
}

void RotatorComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##Rotator_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    float speed[3] = { rotationSpeed_.x, rotationSpeed_.y, rotationSpeed_.z };
    if (ImGui::DragFloat3(("Rotation Speed (rad/s)" + idPrefix).c_str(), speed, kDragSpeed, -100.0f, 100.0f, "%.3f")) {
        rotationSpeed_ = { speed[0], speed[1], speed[2] };
    }

    if (ImGui::Button(("Reset Speed" + idPrefix).c_str())) {
        rotationSpeed_ = { 0.0f, 0.0f, 0.0f };
    }
#endif
}
