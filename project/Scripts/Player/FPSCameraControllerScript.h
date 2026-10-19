#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 一人称レールシューティングカメラ制御スクリプト
/// 自機追従、A/Dキー首振り、被弾シェイク演出を管理する
/// </summary>
class FPSCameraControllerScript : public ScriptComponent {
public:
    FPSCameraControllerScript();
    ~FPSCameraControllerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    // 被弾シェイク発生トリガー
    void TriggerShake();

    // パラメータ更新
    void UpdateFollow(const Vector3& targetPos, const Vector3& tangent);

private:
    // マジックナンバー排除のための定数
    static constexpr float kCameraUpHeight = 1.8f;        // 目の高さオフセット
    static constexpr float kCameraLookAhead = 8.0f;       // 注視点前方距離
    static constexpr float kCameraYawLimit = 0.35f;       // 首振り限界（ラジアン）
    static constexpr float kCameraYawSpeed = 0.015f;      // 首振り旋回速度
    static constexpr float kCameraYawReturnSpeed = 0.02f; // 正面復帰速度
    static constexpr int kShakeDuration = 15;             // シェイク持続フレーム数
    static constexpr float kShakeIntensity = 0.2f;        // シェイク強度
    static constexpr float kLookSmoothFactor = 0.15f;     // 平滑化係数

    // メンバ変数
    float cameraYawOffset_ = 0.0f;
    int shakeTimer_ = 0;
    Vector3 prevLookTangent_ = { 0.0f, 0.0f, 1.0f };
    bool enableLean_ = true;
};
