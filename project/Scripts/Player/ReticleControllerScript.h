#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// マウスカーソルに追従する2D照準（レティクル）制御スクリプト
/// </summary>
class ReticleControllerScript : public ScriptComponent {
public:
    ReticleControllerScript();
    ~ReticleControllerScript() override = default;

    void Awake() override {}
    void Start() override {}
    void OnUpdate() override;
    void OnInspectorGUI() override;

    void SetReticleSize(float size) {
        reticleSize_ = size;
        halfSize_ = size * 0.5f;
    }
    float GetReticleSize() const { return reticleSize_; }

private:
    static constexpr float kDefaultReticleSize = 128.0f;
    static constexpr float kDefaultHalfSize = 64.0f;

    float reticleSize_ = kDefaultReticleSize;
    float halfSize_ = kDefaultHalfSize;
};
