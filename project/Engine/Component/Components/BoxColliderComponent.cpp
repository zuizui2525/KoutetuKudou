#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include "Engine/Math/Matrix/Matrix.h"
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

    OBB obb = GetWorldOBB();

    // モデル表面との埋もれ・Zファイティングを防止するため、外側に微小マージンを付加して展開
    Vector3 extX = Math::Multiply(obb.size.x + kGizmoVisualPadding, obb.orientations[0]);
    Vector3 extY = Math::Multiply(obb.size.y + kGizmoVisualPadding, obb.orientations[1]);
    Vector3 extZ = Math::Multiply(obb.size.z + kGizmoVisualPadding, obb.orientations[2]);

    // OBB の中心と3つの回転軸から8つの頂点を計算
    const std::array<Vector3, kBoxVertexCount> vertices = {{
        // 0: (-x, -y, -z)
        { obb.center.x - extX.x - extY.x - extZ.x, obb.center.y - extX.y - extY.y - extZ.y, obb.center.z - extX.z - extY.z - extZ.z },
        // 1: (+x, -y, -z)
        { obb.center.x + extX.x - extY.x - extZ.x, obb.center.y + extX.y - extY.y - extZ.y, obb.center.z + extX.z - extY.z - extZ.z },
        // 2: (+x, +y, -z)
        { obb.center.x + extX.x + extY.x - extZ.x, obb.center.y + extX.y + extY.y - extZ.y, obb.center.z + extX.z + extY.z - extZ.z },
        // 3: (-x, +y, -z)
        { obb.center.x - extX.x + extY.x - extZ.x, obb.center.y - extX.y + extY.y - extZ.y, obb.center.z - extX.z + extY.z - extZ.z },
        // 4: (-x, -y, +z)
        { obb.center.x - extX.x - extY.x + extZ.x, obb.center.y - extX.y - extY.y + extZ.y, obb.center.z - extX.z - extY.z + extZ.z },
        // 5: (+x, -y, +z)
        { obb.center.x + extX.x - extY.x + extZ.x, obb.center.y + extX.y - extY.y + extZ.y, obb.center.z + extX.z - extY.z + extZ.z },
        // 6: (+x, +y, +z)
        { obb.center.x + extX.x + extY.x + extZ.x, obb.center.y + extX.y + extY.y + extZ.y, obb.center.z + extX.z + extY.z + extZ.z },
        // 7: (-x, +y, +z)
        { obb.center.x - extX.x + extY.x + extZ.x, obb.center.y - extX.y + extY.y + extZ.y, obb.center.z - extX.z + extY.z + extZ.z }
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

OBB BoxColliderComponent::GetWorldOBB() const {
    OBB obb{};
    Vector3 worldPos = { 0.0f, 0.0f, 0.0f };
    Vector3 worldRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 worldScale = { 1.0f, 1.0f, 1.0f };

    if (owner_) {
        worldPos = owner_->GetPosition();
        worldRotate = owner_->GetRotate();
        worldScale = owner_->GetScale();
    }

    // オブジェクトの回転行列を作成
    Matrix4x4 rotMat = Math::MakeRotateMatrix(worldRotate.x, worldRotate.y, worldRotate.z);

    // 回転軸（正規化されたX, Y, Z基底ベクトル）
    obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    // 中心座標（オフセットにも回転を適用）
    Vector3 rotatedOffset = Math::TransformNormal(centerOffset_, rotMat);
    obb.center = {
        worldPos.x + rotatedOffset.x,
        worldPos.y + rotatedOffset.y,
        worldPos.z + rotatedOffset.z
    };

    // 各軸方向の半径（ハーフサイズ）
    obb.size = {
        std::abs(size_.x * worldScale.x) * kHalfFactor,
        std::abs(size_.y * worldScale.y) * kHalfFactor,
        std::abs(size_.z * worldScale.z) * kHalfFactor
    };
    obb.color = kDefaultColliderColor;

    return obb;
}

AABB BoxColliderComponent::GetWorldAABB() const {
    OBB obb = GetWorldOBB();

    Vector3 extX = Math::Multiply(obb.size.x, obb.orientations[0]);
    Vector3 extY = Math::Multiply(obb.size.y, obb.orientations[1]);
    Vector3 extZ = Math::Multiply(obb.size.z, obb.orientations[2]);

    // OBBの8頂点から外接AABBを算出
    Vector3 pMin = {
        obb.center.x - std::abs(extX.x) - std::abs(extY.x) - std::abs(extZ.x),
        obb.center.y - std::abs(extX.y) - std::abs(extY.y) - std::abs(extZ.y),
        obb.center.z - std::abs(extX.z) - std::abs(extY.z) - std::abs(extZ.z)
    };
    Vector3 pMax = {
        obb.center.x + std::abs(extX.x) + std::abs(extY.x) + std::abs(extZ.x),
        obb.center.y + std::abs(extX.y) + std::abs(extY.y) + std::abs(extZ.y),
        obb.center.z + std::abs(extX.z) + std::abs(extY.z) + std::abs(extZ.z)
    };

    AABB aabb{};
    aabb.min = pMin;
    aabb.max = pMax;
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
