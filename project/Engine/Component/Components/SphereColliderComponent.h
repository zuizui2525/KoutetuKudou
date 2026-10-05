#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Math/Collision/CollisionStructs.h"
#include <string>

/// <summary>
/// 球型当たり判定コンポーネント (UnityライクなSphereCollider)
/// </summary>
class SphereColliderComponent : public IComponent {
public:
    SphereColliderComponent() = default;
    ~SphereColliderComponent() override = default;

    void Initialize() override {}
    void Update() override {}
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "SphereCollider"; }

    /// <summary>
    /// ワールド座標系での球を取得
    /// </summary>
    Sphere GetWorldSphere() const;

    // ==========================================
    // パラメータ操作
    // ==========================================
    const Vector3& GetCenterOffset() const { return centerOffset_; }
    void SetCenterOffset(const Vector3& offset) { centerOffset_ = offset; }

    float GetRadius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

    bool IsTrigger() const { return isTrigger_; }
    void SetTrigger(bool trigger) { isTrigger_ = trigger; }

    bool IsShowDebugGizmo() const { return showDebugGizmo_; }
    void SetShowDebugGizmo(bool show) { showDebugGizmo_ = show; }

private:
    Vector3 centerOffset_ = { 0.0f, 0.0f, 0.0f };
    float radius_ = 0.5f;
    bool isTrigger_ = false;
    bool showDebugGizmo_ = true;
};
