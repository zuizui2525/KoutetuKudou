#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// 毎フレーム一定速度で回転し続けるコンポーネント (UnityライクなRotator)
/// 鋼鉄のギアやコアなどの常時駆動オブジェクトにアタッチして使用
/// </summary>
class RotatorComponent : public IComponent {
public:
    RotatorComponent() = default;
    ~RotatorComponent() override = default;

    void Initialize() override {}
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "Rotator"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    const Vector3& GetRotationSpeed() const { return rotationSpeed_; }
    void SetRotationSpeed(const Vector3& speed) { rotationSpeed_ = speed; }

    bool IsDegrees() const { return isDegrees_; }
    void SetDegrees(bool isDegrees) { isDegrees_ = isDegrees; }

private:
    Vector3 rotationSpeed_ = { 0.0f, 0.5f, 0.0f }; // 毎秒の回転量 (デフォルト: Y軸 0.5 rad/s)
    bool isDegrees_ = false;                        // インスペクター表示/指定が度数法かどうか
};
