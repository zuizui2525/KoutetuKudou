#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>

class Line2DObject;

/// <summary>
/// 画面中央固定（APEXスタイル）およびマウスカーソル追従対応の2D照準（レティクル）制御スクリプト
/// 敵命中時のレティクルヒットマーカー（×印）描画機能も提供します。
/// </summary>
class ReticleControllerScript : public ScriptComponent {
public:
    ReticleControllerScript();
    ~ReticleControllerScript() override;

    void Awake() override;
    void Start() override {}
    void OnUpdate() override;
    void Draw2D() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 敵命中時にレティクル中央に「×」印のヒットマーカーを瞬間点灯させます。
    /// </summary>
    /// <param name="isCritical">ヘッドショット（クリティカル）命中なら true</param>
    static void TriggerHitMarker(bool isCritical = false);

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
    void EnsureHitMarkerVisuals();

private:
    static inline ReticleControllerScript* sInstance_ = nullptr;

    // マジックナンバー排除のための定数
    static constexpr float kDefaultReticleSize = 128.0f;
    static constexpr float kDefaultHalfSize = 64.0f;
    static constexpr int kDefaultHitMarkerDuration = 5; // 命中時の×印表示フレーム数 (約0.08秒)
    static constexpr float kHitMarkerGap = 5.0f;        // 中心からの離れ幅 (px)
    static constexpr float kHitMarkerLength = 12.0f;    // 腕の長さ (px)
    static constexpr float kHitMarkerThickness = 2.0f;  // 線の太さ (px)
    static constexpr Vector4 kNormalHitColor = { 1.0f, 1.0f, 1.0f, 0.95f };    // 通常ヒット: 純白
    static constexpr Vector4 kCriticalHitColor = { 1.0f, 0.25f, 0.25f, 1.0f };  // ヘッドショット: 鮮烈な赤

    float reticleSize_ = kDefaultReticleSize;
    float halfSize_ = kDefaultHalfSize;
    bool isLockedToCenter_ = true; // APEXスタイル: 画面中央固定

    // ヒットマーカー制御変数
    int hitMarkerTimer_ = 0;
    bool isHitCritical_ = false;
    std::unique_ptr<Line2DObject> hitMarkerLines_[4]; // 斜め4方向の×印ライン
};
