#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/GameObject.h"
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinSize = 0.01f;
    constexpr float kMaxSize = 1000.0f;
    constexpr float kHalfFactor = 0.5f;
    constexpr unsigned int kDefaultColliderColor = 0x00FF00FF; // 緑色
}

AABB BoxColliderComponent::GetWorldAABB() const {
    Vector3 worldCenter = centerOffset_;
    Vector3 worldScale = { 1.0f, 1.0f, 1.0f };

    if (owner_) {
        worldCenter.x += owner_->GetPosition().x;
        worldCenter.y += owner_->GetPosition().y;
        worldCenter.z += owner_->GetPosition().z;

        worldScale = owner_->GetScale();
    }

    Vector3 halfSize = {
        std::abs(size_.x * worldScale.x) * kHalfFactor,
        std::abs(size_.y * worldScale.y) * kHalfFactor,
        std::abs(size_.z * worldScale.z) * kHalfFactor
    };

    AABB aabb{};
    aabb.min = { worldCenter.x - halfSize.x, worldCenter.y - halfSize.y, worldCenter.z - halfSize.z };
    aabb.max = { worldCenter.x + halfSize.x, worldCenter.y + halfSize.y, worldCenter.z + halfSize.z };
    aabb.color = kDefaultColliderColor;
    return aabb;
}

void BoxColliderComponent::DrawInspector() {
#ifdef _USEIMGUI
    // 1. Center Offset
    float center[3] = { centerOffset_.x, centerOffset_.y, centerOffset_.z };
    if (ImGui::DragFloat3("Center##BoxCollider", center, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        centerOffset_ = { center[0], center[1], center[2] };
    }

    // 2. Size
    float size[3] = { size_.x, size_.y, size_.z };
    if (ImGui::DragFloat3("Size##BoxCollider", size, kDragSpeed, kMinSize, kMaxSize, "%.2f")) {
        size_ = { size[0], size[1], size[2] };
    }

    // 3. Is Trigger
    bool trigger = isTrigger_;
    if (ImGui::Checkbox("Is Trigger##BoxCollider", &trigger)) {
        isTrigger_ = trigger;
    }

    // 4. Show Debug Gizmo
    bool showGizmo = showDebugGizmo_;
    if (ImGui::Checkbox("Show Gizmo##BoxCollider", &showGizmo)) {
        showDebugGizmo_ = showGizmo;
    }
#endif
}
