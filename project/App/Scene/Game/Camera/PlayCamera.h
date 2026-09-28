#pragma once
#include <memory>
#include <string>
#include "Engine/Math/MathStructs.h"

class BaseCamera;
class CameraManager;
class Input;

/**
 * @brief 走行プレイモード用のメインカメラ制御クラス
 */
class PlayCamera {
public:
    PlayCamera();
    ~PlayCamera();

    // 初期化処理
    void Initialize(CameraManager* cameraMgr);

    // 更新処理
    void Update(Input* input, const Vector3& playerPos, const Vector3& tangent, float currentDistance);

    // カメラ位置・回転をプレイ開始状態にリセット
    void Reset(const Vector3& playerPos, const Vector3& tangent);

    // 被弾シェイクのトリガー
    void TriggerShake();

    // ゲッター
    BaseCamera* GetCamera() const { return mainCamera_.get(); }

private:
    // マジックナンバー排除のための定数
    static inline const Vector3 kDefaultCameraPos = { 0.0f, 4.0f, -20.0f }; // デフォルト位置
    static inline const int kShakeDuration = 15;                             // シェイク時間
    static inline const float kShakeIntensity = 0.2f;                        // シェイクの強さ
    static inline const float kCameraYawLimit = 0.35f;                       // 首振り限界（ラジアン）
    static inline const float kCameraYawSpeed = 0.015f;                      // 首振り速度
    static inline const float kCameraYawReturnSpeed = 0.02f;                 // 首振り戻り速度
    static inline const float kCameraUpHeight = 1.8f;                        // 目の高さのYオフセット
    static inline const float kCameraLookAhead = 8.0f;                       // 前方への注視点オフセット
    static inline const float kLookSmoothFactor = 0.15f;                     // カメラ注視方向の平滑化係数（Lerp）
    static inline const std::string kMainCameraName = "Main";               // 登録用キー

private:
    std::shared_ptr<BaseCamera> mainCamera_;                 // メインカメラ
    float cameraYawOffset_ = 0.0f;                           // A/Dキーでのカメラ首振りヨー角オフセット
    int shakeTimer_ = 0;                                     // 被弾時のカメラシェイクタイマー
    Vector3 prevLookTangent_ = { 0.0f, 0.0f, 1.0f };         // 前フレームの注視方向ベクトル
    CameraManager* cameraMgr_ = nullptr;
};
