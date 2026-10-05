#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/GameObject.h"
#include <algorithm>
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinRadius = 0.01f;
    constexpr float kMaxRadius = 500.0f;
    constexpr unsigned int kDefaultColliderColor = 0x00FF00FF; // 緑色
}

Sphere SphereColliderComponent::GetWorldSphere() const {
    Vector3 worldCenter = centerOffset_;
    float maxScale = 1.0f;

    if (owner_) {
        worldCenter.x += owner_->GetPosition().x;
        worldCenter.y += owner_->GetPosition().y;
        worldCenter.z += owner_->GetPosition().z;

        const Vector3& s = owner_->GetScale();
        maxScale = (std::max)({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
    }

    Sphere sphere{};
    sphere.center = worldCenter;
    sphere.radius = radius_ * maxScale;
    sphere.color = kDefaultColliderColor;
    return sphere;
}

void SphereColliderComponent::DrawInspector() {
#ifdef _USEIMGUI
    // 1. Center Offset
    float center[3] = { centerOffset_.x, centerOffset_.y, centerOffset_.z };
    if (ImGui::DragFloat3("Center##SphereCollider", center, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        centerOffset_ = { center[0], center[1], center[2] };
    }

    // 2. Radius
    float r = radius_;
    if (ImGui::DragFloat("Radius##SphereCollider", &r, kDragSpeed, kMinRadius, kMaxRadius, "%.2f")) {
        radius_ = r;
    }

    // 3. Is Trigger
    bool trigger = isTrigger_;
    if (ImGui::Checkbox("Is Trigger##SphereCollider", &trigger)) {
        isTrigger_ = trigger;
    }

    // 4. Show Debug Gizmo
    bool showGizmo = showDebugGizmo_;
    if (ImGui::Checkbox("Show Gizmo##SphereCollider", &showGizmo)) {
        showDebugGizmo_ = showGizmo;
    }
#endif
}
