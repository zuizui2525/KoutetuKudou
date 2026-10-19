#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinRadius = 0.01f;
    constexpr float kMaxRadius = 500.0f;
    constexpr float kMinThickness = 0.005f;
    constexpr float kMaxThickness = 0.5f;
    constexpr unsigned int kDefaultColliderColor = 0x00FF00FF; // 緑色

    // ギズモ描画定数
    constexpr size_t kSegmentsPerRing = 16;
    constexpr size_t kRingCount = 3;
    constexpr size_t kTotalSphereLines = kSegmentsPerRing * kRingCount;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    const Vector4 kNormalWireColor = { 0.2f, 1.0f, 0.2f, 0.9f };  // 通常コライダー: 緑
    const Vector4 kTriggerWireColor = { 1.0f, 0.85f, 0.1f, 0.9f }; // トリガー: 黄色
    const std::string kGizmoTextureKey = "white";

    void SyncGPUForDeletion() {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }
}

SphereColliderComponent::SphereColliderComponent() = default;

SphereColliderComponent::~SphereColliderComponent() {
    SyncGPUForDeletion();
    gizmoLines_.clear();
}

void SphereColliderComponent::EnsureGizmoLines() {
    if (gizmoLines_.size() != kTotalSphereLines) {
        gizmoLines_.clear();
        gizmoLines_.reserve(kTotalSphereLines);
        for (size_t i = 0; i < kTotalSphereLines; ++i) {
            auto line = std::make_unique<LineObject>();
            line->SetAutoRegisterHierarchy(false);
            line->Initialize();
            line->SetThickness(gizmoWireThickness_);
            gizmoLines_.push_back(std::move(line));
        }
    }
}

void SphereColliderComponent::Draw() {
    if (!showDebugGizmo_ || !isActive_) return;

    EnsureGizmoLines();

    Sphere sphere = GetWorldSphere();
    const Vector3& center = sphere.center;
    const float radius = sphere.radius;
    const Vector4& wireColor = isTrigger_ ? kTriggerWireColor : kNormalWireColor;

    size_t lineIdx = 0;

    // 1. XY 平面リング (Z = 0)
    for (size_t i = 0; i < kSegmentsPerRing; ++i) {
        float theta0 = (static_cast<float>(i) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;
        float theta1 = (static_cast<float>(i + 1) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;

        Vector3 p0 = { center.x + radius * std::cos(theta0), center.y + radius * std::sin(theta0), center.z };
        Vector3 p1 = { center.x + radius * std::cos(theta1), center.y + radius * std::sin(theta1), center.z };

        auto& line = gizmoLines_[lineIdx++];
        if (line) {
            line->SetStartPoint(p0);
            line->SetEndPoint(p1);
            line->SetThickness(gizmoWireThickness_);
            line->SetColor(wireColor);
            line->Update();
            line->Draw(kGizmoTextureKey);
        }
    }

    // 2. XZ 平面リング (Y = 0)
    for (size_t i = 0; i < kSegmentsPerRing; ++i) {
        float theta0 = (static_cast<float>(i) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;
        float theta1 = (static_cast<float>(i + 1) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;

        Vector3 p0 = { center.x + radius * std::cos(theta0), center.y, center.z + radius * std::sin(theta0) };
        Vector3 p1 = { center.x + radius * std::cos(theta1), center.y, center.z + radius * std::sin(theta1) };

        auto& line = gizmoLines_[lineIdx++];
        if (line) {
            line->SetStartPoint(p0);
            line->SetEndPoint(p1);
            line->SetThickness(gizmoWireThickness_);
            line->SetColor(wireColor);
            line->Update();
            line->Draw(kGizmoTextureKey);
        }
    }

    // 3. YZ 平面リング (X = 0)
    for (size_t i = 0; i < kSegmentsPerRing; ++i) {
        float theta0 = (static_cast<float>(i) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;
        float theta1 = (static_cast<float>(i + 1) / static_cast<float>(kSegmentsPerRing)) * kTwoPi;

        Vector3 p0 = { center.x, center.y + radius * std::cos(theta0), center.z + radius * std::sin(theta0) };
        Vector3 p1 = { center.x, center.y + radius * std::cos(theta1), center.z + radius * std::sin(theta1) };

        auto& line = gizmoLines_[lineIdx++];
        if (line) {
            line->SetStartPoint(p0);
            line->SetEndPoint(p1);
            line->SetThickness(gizmoWireThickness_);
            line->SetColor(wireColor);
            line->Update();
            line->Draw(kGizmoTextureKey);
        }
    }
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

    // 5. Gizmo Thickness
    if (showDebugGizmo_) {
        float thickness = gizmoWireThickness_;
        if (ImGui::DragFloat("Gizmo Thickness##SphereCollider", &thickness, 0.005f, kMinThickness, kMaxThickness, "%.3f")) {
            gizmoWireThickness_ = thickness;
        }
    }
#endif
}
