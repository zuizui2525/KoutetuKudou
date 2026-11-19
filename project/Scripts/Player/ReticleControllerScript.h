#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>

class Line2DObject;
class Ring2DObject;

/// <summary>
/// 画面中央固定（APEXスタイル）およびマウスカーソル追従対応の2D照準（レティクル）制御スクリプト
/// 敵命中時にレティクル外周へ弾けるヒット確認リング＆4方向ワイドティックを描画します。
/// </summary>
class ReticleControllerScript : public ScriptComponent {
public:
    ReticleControllerScript();
    ~ReticleControllerScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw2D() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 敵命中時にレティクル外周のヒット確認リング＆ワイドティックを点灯・弾けアニメーションさせます。
    /// </summary>
    /// <param name="isCritical">ヘッドショット（クリティカル）命中なら true</param>
    static void TriggerHitMarker(bool isCritical = false);

    /// <summary>
    /// レティクルの描画サイズ（正方形の一辺長）を設定し、SpriteRendererのサイズと描画オフセットを自動同期します。
    /// </summary>
    /// <param name="size">レティクルの幅および高さ（px）</param>
    void SetReticleSize(float size);

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
    static constexpr float kDefaultReticleSize = 52.0f;      // シャープなFPS標準レティクルサイズ (px: 半径26px)
    static constexpr float kDefaultHalfSize = 26.0f;         // レティクル半長 (px)
    static constexpr int kDefaultHitMarkerDuration = 10;     // 命中演出の持続フレーム数 (約0.17秒)
    static constexpr float kHitRingStartRadius = 14.0f;      // ヒット確認リング開始半径 (px)
    static constexpr float kHitRingEndRadius = 38.0f;        // 通常ヒット確認リング終了半径 (px: レティクル外周を突き抜けて広がる)
    static constexpr float kCriticalRingStartRadius = 16.0f; // クリティカル確認リング開始半径 (px)
    static constexpr float kCriticalRingEndRadius = 46.0f;   // クリティカル確認リング終了半径 (px)
    static constexpr float kHitRingThickness = 2.8f;         // 通常確認リングの太さ (px)
    static constexpr float kCriticalRingThickness = 3.6f;    // クリティカル確認リングの太さ (px)
    static constexpr float kHitTickBaseGap = 15.0f;          // 外周4方向ティックの中心からの基準離れ幅 (px)
    static constexpr float kHitTickPopDist = 10.0f;          // 命中瞬間の外側弾け移動距離 (px)
    static constexpr float kHitTickLength = 12.0f;           // 通常ヒットティック長さ (px)
    static constexpr float kCriticalTickLength = 16.0f;      // クリティカルティック長さ (px)
    static constexpr float kHitTickThickness = 3.2f;         // 通常ヒットティック太さ (px)
    static constexpr float kCriticalTickThickness = 4.0f;    // クリティカルティック太さ (px)
    static constexpr Vector4 kNormalHitColor = { 1.0f, 0.88f, 0.20f, 1.0f };    // 通常ヒット: 鮮明なゴールドイエロー
    static constexpr Vector4 kCriticalHitColor = { 1.0f, 0.20f, 0.22f, 1.0f };  // ヘッドショット: 鮮烈レッド

    float reticleSize_ = kDefaultReticleSize;
    float halfSize_ = kDefaultHalfSize;
    bool isLockedToCenter_ = true; // APEXスタイル: 画面中央固定

    // ヒットフィードバック制御変数
    int hitMarkerTimer_ = 0;
    bool isHitCritical_ = false;
    std::unique_ptr<Ring2DObject> hitConfirmRing_;      // レティクル外周へ弾けるヒット確認リング
    std::unique_ptr<Line2DObject> hitMarkerLines_[4];   // 外周4方向ワイドティック
};
