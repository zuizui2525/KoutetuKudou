#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 一人称レールシューティングカメラ制御スクリプト
/// 自機追従、APEX風ダイレクト視点旋回（全周360度見渡し）、被弾シェイク演出を管理する
/// </summary>
class FPSCameraControllerScript : public ScriptComponent {
public:
    FPSCameraControllerScript();
    ~FPSCameraControllerScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 被弾時などの画面揺れ（カメラシェイク）演出をトリガーします。
    /// </summary>
    void TriggerShake();

    /// <summary>
    /// 追従対象（自機）の現在座標およびレール進行方向接線ベクトルに基づき、
    /// マウス連動視線トラッキング、視線スムージング、カメラ座標・回転を更新します。
    /// </summary>
    /// <param name="targetPos">自機の3Dワールド座標</param>
    /// <param name="tangent">自機の走行進行方向ベクトル</param>
    void UpdateFollow(const Vector3& targetPos, const Vector3& tangent);

    float GetCameraYawOffset() const { return cameraYawOffset_; }
    float GetCameraPitchOffset() const { return cameraPitchOffset_; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    void SetMouseSensitivity(float sens) { mouseSensitivity_ = sens; }

private:
    // マジックナンバー排除のための定数
    static constexpr float kCameraUpHeight = 1.8f;              // 目の高さオフセット
    static constexpr float kCameraLookAhead = 8.0f;             // 注視点前方距離
    static constexpr float kCameraYawLimitHalf = 3.14159265f;   // 全周360度視点旋回制限（左右最大各180度 = π rad）
    static constexpr float kMaxPitchLimit = 1.30f;              // 上下最大見上げ・見下ろし角 (約75度 = 1.30 rad)
    static constexpr float kDefaultMouseSensitivity = 0.0022f;  // APEX風マウス操作時の標準感度 (rad/count)
    static constexpr float kMinMouseSensitivity = 0.0005f;      // 最小マウス感度
    static constexpr float kMaxMouseSensitivity = 0.0100f;      // 最大マウス感度
    static constexpr float kCameraPitchLimit = 0.2617994f;      // 画面端追従時の上下首振り制限
    static constexpr float kMouseTrackSmoothSpeed = 0.12f;      // 画面端追従時のスムージング補間速度
    static constexpr float kAimCurveExponent = 2.0f;           // 中央付近のエイム安定化指数カーブ
    static constexpr float kCameraYawSpeed = 0.015f;            // A/Dキー首振り旋回速度 (キー併用時)
    static constexpr float kCameraYawReturnSpeed = 0.04f;       // 正面復帰速度
    static constexpr int kShakeDuration = 15;                   // シェイク持続フレーム数
    static constexpr float kShakeIntensity = 0.2f;              // シェイク強度
    static constexpr float kLookSmoothFactor = 0.15f;           // 平滑化係数

    // 死亡シネマティック演出用の定数
    static constexpr float kDeathCameraFrontDist = 5.2f;      // 正面カメラの前方距離（自機全体を綺麗に収める距離に拡大）
    static constexpr float kDeathCameraFrontHeight = 1.6f;    // 正面カメラの高さ（見下ろし感を保つ自然な高さ）
    static constexpr float kDeathCameraSideSwing = 1.4f;      // 正面へ回り込む横膨らみ量（距離に応じた優雅なアーク軌道）
    static constexpr float kDeathCameraLerpSpeed = 0.08f;     // 正面回り込みのスムーズ補間係数
    static constexpr float kDeathTargetCenterHeight = 0.8f;   // 注視対象の自機中心高さ

    // メンバ変数
    float cameraYawOffset_ = 0.0f;
    float cameraPitchOffset_ = 0.0f;
    float mouseSensitivity_ = kDefaultMouseSensitivity;
    bool useDirectAim_ = true; // APEXスタイル: マウス移動量によるダイレクトエイム
    int shakeTimer_ = 0;
    Vector3 prevLookTangent_ = { 0.0f, 0.0f, 1.0f };
    bool enableLean_ = true;
    bool enableMouseTracking_ = true;

    // 死亡シネマティック正面カメラワーク進行度 (0.0f: 通常一人称 -> 1.0f: 正面到達)
    float deathCinematicProgress_ = 0.0f;

    // ゲーム中カーソル中央固定・非表示管理
    bool isCursorLocked_ = false;
    void UpdateCursorLock(bool shouldLock);
};
