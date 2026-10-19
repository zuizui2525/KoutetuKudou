#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Math/Collision/CollisionStructs.h"
#include <string>
#include <vector>
#include <memory>

class LineObject;

/// <summary>
/// 球型当たり判定コンポーネント (UnityライクなSphereCollider)
/// </summary>
class SphereColliderComponent : public IComponent {
public:
    SphereColliderComponent();
    ~SphereColliderComponent() override;

    void Initialize() override {}
    void Update() override {}
    void Draw() override;
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

    float GetGizmoWireThickness() const { return gizmoWireThickness_; }
    void SetGizmoWireThickness(float thickness) { gizmoWireThickness_ = thickness; }

private:
    void EnsureGizmoLines();

private:
    Vector3 centerOffset_ = { 0.0f, 0.0f, 0.0f };
    float radius_ = 0.5f;
    bool isTrigger_ = false;
    bool showDebugGizmo_ = true;
    float gizmoWireThickness_ = 0.02f;

    // 3軸リング描画用ラインオブジェクト
    std::vector<std::unique_ptr<LineObject>> gizmoLines_;
};
