#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <array>
#include <utility>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinSize = 0.01f;
    constexpr float kMaxSize = 1000.0f;
    constexpr float kHalfFactor = 0.5f;
    constexpr float kMinThickness = 0.005f;
    constexpr float kMaxThickness = 0.5f;
    constexpr unsigned int kDefaultColliderColor = 0x00FF00FF; // 緑色

    // ギズモ描画定数
    constexpr size_t kBoxEdgeCount = 12;
    constexpr size_t kBoxVertexCount = 8;
    const Vector4 kNormalWireColor = { 0.2f, 1.0f, 0.2f, 0.9f };  // 通常コライダー: 緑
    const Vector4 kTriggerWireColor = { 1.0f, 0.85f, 0.1f, 0.9f }; // トリガー: 黄色
    const Vector4 kCollidingWireColor = { 1.0f, 0.2f, 0.2f, 0.95f }; // 接触中: 赤
    constexpr float kGizmoVisualPadding = 0.02f; // モデル表面との重なり・埋もれを防止する視認用マージン
    const std::string kGizmoTextureKey = "white";

    // AABBの12本のエッジ接続インデックス
    constexpr std::pair<size_t, size_t> kBoxEdges[kBoxEdgeCount] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // 前面/底面四辺
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // 背面/上面四辺
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }  // 結合四辺
    };

    void SyncGPUForDeletion() {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }
}

BoxColliderComponent::BoxColliderComponent() = default;

BoxColliderComponent::~BoxColliderComponent() {
    SyncGPUForDeletion();
    gizmoLines_.clear();
}

void BoxColliderComponent::EnsureGizmoLines() {
    if (gizmoLines_.size() != kBoxEdgeCount) {
        gizmoLines_.clear();
        gizmoLines_.reserve(kBoxEdgeCount);
        for (size_t i = 0; i < kBoxEdgeCount; ++i) {
            auto line = std::make_unique<LineObject>();
            line->SetAutoRegisterHierarchy(false);
            line->Initialize();
            line->SetThickness(gizmoWireThickness_);
            gizmoLines_.push_back(std::move(line));
        }
    }
}

void BoxColliderComponent::Draw() {
    if (!showDebugGizmo_ || !isActive_) return;

    EnsureGizmoLines();

    AABB aabb = GetWorldAABB();
    // モデルと重なって見えなくなるのを防ぐため、わずかに外側にマージンを持たせて描画
    const Vector3 pMin = {
        aabb.min.x - kGizmoVisualPadding,
        aabb.min.y - kGizmoVisualPadding,
        aabb.min.z - kGizmoVisualPadding
    };
    const Vector3 pMax = {
        aabb.max.x + kGizmoVisualPadding,
        aabb.max.y + kGizmoVisualPadding,
        aabb.max.z + kGizmoVisualPadding
    };

    const std::array<Vector3, kBoxVertexCount> vertices = {{
        { pMin.x, pMin.y, pMin.z }, // 0
        { pMax.x, pMin.y, pMin.z }, // 1
        { pMax.x, pMax.y, pMin.z }, // 2
        { pMin.x, pMax.y, pMin.z }, // 3
        { pMin.x, pMin.y, pMax.z }, // 4
        { pMax.x, pMin.y, pMax.z }, // 5
        { pMax.x, pMax.y, pMax.z }, // 6
        { pMin.x, pMax.y, pMax.z }  // 7
    }};

    const Vector4& wireColor = isColliding_ ? kCollidingWireColor :
                               (isTrigger_ ? kTriggerWireColor : kNormalWireColor);

    for (size_t i = 0; i < kBoxEdgeCount; ++i) {
        auto& line = gizmoLines_[i];
        if (!line) continue;

        const auto& edge = kBoxEdges[i];
        line->SetStartPoint(vertices[edge.first]);
        line->SetEndPoint(vertices[edge.second]);
        line->SetThickness(gizmoWireThickness_);
        line->SetColor(wireColor);
        line->Update();
        line->Draw(kGizmoTextureKey);
    }
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

    // 5. Gizmo Thickness
    if (showDebugGizmo_) {
        float thickness = gizmoWireThickness_;
        if (ImGui::DragFloat("Gizmo Thickness##BoxCollider", &thickness, 0.005f, kMinThickness, kMaxThickness, "%.3f")) {
            gizmoWireThickness_ = thickness;
        }
    }
#endif
}
