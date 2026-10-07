#pragma once
#include "Engine/Component/IComponent.h"
#include <string>

/// <summary>
/// 2Dテキストやスプライトの不透明度（Alpha）を周期的に明滅・点滅させるコンポーネント
/// 「PRESS SPACE TO START」等のキー案内UIにアタッチして使用
/// </summary>
class BlinkComponent : public IComponent {
public:
    BlinkComponent() = default;
    ~BlinkComponent() override = default;

    void Initialize() override {}
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "Blink"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    float GetBlinkSpeed() const { return blinkSpeed_; }
    void SetBlinkSpeed(float speed) { blinkSpeed_ = speed; }

    float GetMinAlpha() const { return minAlpha_; }
    void SetMinAlpha(float minAlpha) { minAlpha_ = minAlpha; }

    float GetMaxAlpha() const { return maxAlpha_; }
    void SetMaxAlpha(float maxAlpha) { maxAlpha_ = maxAlpha; }

private:
    float blinkSpeed_ = 3.0f; // 点滅速度 (rad/s, 約0.5Hz)
    float minAlpha_ = 0.15f;  // 最小不透明度 (消え去らずにほんのり残す)
    float maxAlpha_ = 1.0f;   // 最大不透明度
    float timer_ = 0.0f;      // 経過タイマー
};
