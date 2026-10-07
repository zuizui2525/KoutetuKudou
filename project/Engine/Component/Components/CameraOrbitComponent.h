#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// カメラを一定の中心点周りで自動周回（オービット）移動させるコンポーネント
/// タイトルシーンのダイナミックかつ重厚な背景演出にアタッチして使用
/// </summary>
class CameraOrbitComponent : public IComponent {
public:
    CameraOrbitComponent() = default;
    ~CameraOrbitComponent() override = default;

    void Initialize() override;
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "CameraOrbit"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    const Vector3& GetTargetCenter() const { return targetCenter_; }
    void SetTargetCenter(const Vector3& center) { targetCenter_ = center; }

    float GetRadius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

    float GetHeight() const { return height_; }
    void SetHeight(float height) { height_ = height; }

    float GetOrbitSpeed() const { return orbitSpeed_; }
    void SetOrbitSpeed(float speed) { orbitSpeed_ = speed; }

    float GetCurrentAngle() const { return currentAngle_; }
    void SetCurrentAngle(float angle) { currentAngle_ = angle; }

private:
    Vector3 targetCenter_ = { 0.0f, 0.0f, 0.0f }; // 注視する中心座標
    float radius_ = 25.0f;                        // 周回半径
    float height_ = 4.0f;                         // カメラの高さ (Y座標オフセット)
    float orbitSpeed_ = 0.15f;                    // 角速度 (rad/s)
    float currentAngle_ = 0.0f;                   // 現在の角度 (rad)
};
