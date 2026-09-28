#pragma once
#include <memory>
#include <vector>
#include "Engine/Math/MathStructs.h"

class BaseCamera;
class CameraManager;
class LineObject;
class Input;

/**
 * @brief ルート描画モード用のズームカメラ制御クラス
 */
class DrawRouteCamera {
public:
    DrawRouteCamera();
    ~DrawRouteCamera();

    // 初期化処理
    void Initialize(CameraManager* cameraMgr, float startZ);

    // 更新処理
    void Update(Input* input, float startZ, float goalZ);

    // ゲッター・セッター
    BaseCamera* GetCamera() const { return cameraZoom_.get(); }
    const Vector3& GetTargetZoom() const { return targetZoom_; }
    const Vector3& GetDestinationZoom() const { return destinationZoom_; }
    void SetTargetZoom(const Vector3& target);
    void SetDestinationZoom(const Vector3& target);

private:
    // マジックナンバー排除のための定数
    static inline const Vector3 kZoomCameraOffset = { 0.0f, 25.0f, -20.0f }; // ズームカメラの注視点からのオフセット
    static inline const float kZoomScrollSpeed = 0.15f;                      // ズームカメラの右ドラッグスクロール速度
    static inline const float kMapLimitX = 15.0f;                           // マップ左右の限界値
    static inline const float kFrameLineThickness = 0.3f;                   // 視野可視化枠線の太さ
    static inline const float kMinZoom = 0.3f;                              // 最小ズーム率
    static inline const float kMaxZoom = 2.0f;                              // 最大ズーム率
    static inline const float kZoomSensitivity = 0.001f;                    // ホイールズーム感度
    static inline const float kLerpRate = 0.1f;                             // カメラ補間（イージング）率
    static inline const float kZoomLerpRate = 0.15f;                        // ズーム補間率

private:
    std::shared_ptr<BaseCamera> cameraZoom_;                 // ズームカメラ
    Vector3 targetZoom_ = { 0.0f, 0.0f, 0.0f };              // ズームカメラの現在の注視点
    Vector3 destinationZoom_ = { 0.0f, 0.0f, 0.0f };         // ズームカメラの目標注視点
    float zoomFactor_ = 1.0f;                                // 現在のズーム倍率
    float targetZoomFactor_ = 1.0f;                          // 目標のズーム倍率
    bool isEasing_ = false;                                  // 中央戻りイージング中か
    std::vector<std::unique_ptr<LineObject>> zoomFrameLines_; // 左画面に描画するカメラ視野可視化枠線（3D）
    CameraManager* cameraMgr_ = nullptr;
};
