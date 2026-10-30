#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 画面中央固定（APEXスタイル）およびマウスカーソル追従対応の2D照準（レティクル）制御スクリプト
/// </summary>
class ReticleControllerScript : public ScriptComponent {
public:
    ReticleControllerScript();
    ~ReticleControllerScript() override = default;

    void Awake() override {}
    void Start() override {}
    void OnUpdate() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// レティクルの描画サイズ（正方形の一辺長）を設定し、描画オフセット用の半長を自動計算します。
    /// </summary>
    /// <param name="size">レティクルの幅および高さ（px）</param>
    void SetReticleSize(float size) {
        reticleSize_ = size;
        halfSize_ = size * 0.5f;
    }

    /// <summary>
    /// レティクルの描画サイズを取得します。
    /// </summary>
    /// <returns>レティクルサイズ（px）</returns>
    float GetReticleSize() const { return reticleSize_; }

    void SetLockToCenter(bool lock) { isLockedToCenter_ = lock; }
    bool IsLockedToCenter() const { return isLockedToCenter_; }

private:
    static constexpr float kDefaultReticleSize = 128.0f;
    static constexpr float kDefaultHalfSize = 64.0f;

    float reticleSize_ = kDefaultReticleSize;
    float halfSize_ = kDefaultHalfSize;
    bool isLockedToCenter_ = true; // APEXスタイル: 画面中央固定
};
