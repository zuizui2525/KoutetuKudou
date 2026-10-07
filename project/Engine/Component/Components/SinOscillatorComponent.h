#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// Sin波に基づいて位置、回転、またはスケールを周期的に往復・浮遊させるコンポーネント
/// タイトルロゴの浮遊や、メカニカルな往復運動にアタッチして使用
/// </summary>
class SinOscillatorComponent : public IComponent {
public:
    enum class TargetType {
        Position, // 位置の往復
        Rotation, // 回転の往復
        Scale     // スケールの脈動
    };

    SinOscillatorComponent() = default;
    ~SinOscillatorComponent() override = default;

    void Initialize() override;
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "SinOscillator"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    TargetType GetTargetType() const { return targetType_; }
    void SetTargetType(TargetType type) { targetType_ = type; }

    const Vector3& GetAmplitude() const { return amplitude_; }
    void SetAmplitude(const Vector3& amplitude) { amplitude_ = amplitude; }

    float GetFrequency() const { return frequency_; }
    void SetFrequency(float frequency) { frequency_ = frequency; }

    float GetPhase() const { return phase_; }
    void SetPhase(float phase) { phase_ = phase; }

    const Vector3& GetBaseValue() const { return baseValue_; }
    void SetBaseValue(const Vector3& base) { baseValue_ = base; isBaseCaptured_ = true; }

    void CaptureBaseValue();

private:
    TargetType targetType_ = TargetType::Position;
    Vector3 amplitude_ = { 0.0f, 10.0f, 0.0f }; // 振幅 (デフォルト: Y軸方向に10px/unit)
    float frequency_ = 2.0f;                    // 周波数 (rad/s)
    float phase_ = 0.0f;                        // 初期位相 (rad)

    Vector3 baseValue_ = { 0.0f, 0.0f, 0.0f };  // 振動の基準値
    bool isBaseCaptured_ = false;               // 基準値が取得されたか
    float timer_ = 0.0f;                        // 経過タイマー
};
