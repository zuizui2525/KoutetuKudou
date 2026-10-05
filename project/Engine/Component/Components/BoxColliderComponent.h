#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Math/Collision/CollisionStructs.h"
#include <string>

/// <summary>
/// 直方体 (AABB) 当たり判定コンポーネント (UnityライクなBoxCollider)
/// </summary>
class BoxColliderComponent : public IComponent {
public:
    BoxColliderComponent() = default;
    ~BoxColliderComponent() override = default;

    void Initialize() override {}
    void Update() override {}
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "BoxCollider"; }

    /// <summary>
    /// ワールド座標系でのAABBを取得
    /// </summary>
    AABB GetWorldAABB() const;

    // ==========================================
    // パラメータ操作
    // ==========================================
    const Vector3& GetCenterOffset() const { return centerOffset_; }
    void SetCenterOffset(const Vector3& offset) { centerOffset_ = offset; }

    const Vector3& GetSize() const { return size_; }
    void SetSize(const Vector3& size) { size_ = size; }

    bool IsTrigger() const { return isTrigger_; }
    void SetTrigger(bool trigger) { isTrigger_ = trigger; }

    bool IsShowDebugGizmo() const { return showDebugGizmo_; }
    void SetShowDebugGizmo(bool show) { showDebugGizmo_ = show; }

private:
    Vector3 centerOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
    bool isTrigger_ = false;
    bool showDebugGizmo_ = true;
};
