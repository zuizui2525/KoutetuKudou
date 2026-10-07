#include "Engine/Component/Components/CameraOrbitComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/CameraComponent.h"
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // デルタタイム定数（60FPS基準: マジックナンバー排除）
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    // インスペクター編集用定数
    constexpr float kDragSpeed = 0.05f;
    constexpr float kAngleDragSpeed = 0.02f;
    constexpr float kTwoPi = 6.283185307f;
}

void CameraOrbitComponent::Initialize() {
    if (auto* cam = owner_->GetComponent<CameraComponent>()) {
        cam->SetUseTarget(true);
        cam->SetTarget(targetCenter_);
    }
}

void CameraOrbitComponent::Update() {
    if (!owner_ || !isActive_) return;

    currentAngle_ += orbitSpeed_ * kDefaultDeltaTime;
    if (std::abs(currentAngle_) > kTwoPi) {
        currentAngle_ = std::fmod(currentAngle_, kTwoPi);
    }

    // 中心点周りの円周座標を計算
    Vector3 newPos = {
        targetCenter_.x + radius_ * std::sin(currentAngle_),
        targetCenter_.y + height_,
        targetCenter_.z - radius_ * std::cos(currentAngle_)
    };
    owner_->SetPosition(newPos);

    // 中心点への視線ベクトルとオイラー角の計算 (マジックナンバー排除)
    Vector3 dir = {
        targetCenter_.x - newPos.x,
        targetCenter_.y - newPos.y,
        targetCenter_.z - newPos.z
    };
    constexpr float kZero = 0.0f;
    constexpr float kEpsilon = 0.0001f;
    float horizontalDist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    float pitch = (horizontalDist < kEpsilon && std::abs(dir.y) < kEpsilon) ? kZero : -std::atan2(dir.y, horizontalDist);
    float yaw = (horizontalDist < kEpsilon && std::abs(dir.y) < kEpsilon) ? kZero : std::atan2(dir.x, dir.z);
    owner_->SetRotate({ pitch, yaw, kZero });

    // カメラコンポーネントがある場合は注視点も設定
    if (auto* cam = owner_->GetComponent<CameraComponent>()) {
        cam->SetUseTarget(true);
        cam->SetTarget(targetCenter_);
    }
}

void CameraOrbitComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##CameraOrbit_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    float center[3] = { targetCenter_.x, targetCenter_.y, targetCenter_.z };
    if (ImGui::DragFloat3(("Target Center" + idPrefix).c_str(), center, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        targetCenter_ = { center[0], center[1], center[2] };
    }

    float rad = radius_;
    if (ImGui::DragFloat(("Radius" + idPrefix).c_str(), &rad, kDragSpeed, 1.0f, 500.0f, "%.2f")) {
        radius_ = rad;
    }

    float h = height_;
    if (ImGui::DragFloat(("Height" + idPrefix).c_str(), &h, kDragSpeed, -100.0f, 100.0f, "%.2f")) {
        height_ = h;
    }

    float speed = orbitSpeed_;
    if (ImGui::DragFloat(("Orbit Speed (rad/s)" + idPrefix).c_str(), &speed, kAngleDragSpeed, -10.0f, 10.0f, "%.3f")) {
        orbitSpeed_ = speed;
    }

    float angle = currentAngle_;
    if (ImGui::DragFloat(("Current Angle" + idPrefix).c_str(), &angle, kAngleDragSpeed, -3.14f, 3.14f, "%.2f")) {
        currentAngle_ = angle;
    }
#endif
}
