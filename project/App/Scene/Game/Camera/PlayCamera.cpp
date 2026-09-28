#include "App/Scene/Game/Camera/PlayCamera.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Input/Input.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

PlayCamera::PlayCamera() = default;
PlayCamera::~PlayCamera() = default;

void PlayCamera::Initialize(CameraManager* cameraMgr) {
    cameraMgr_ = cameraMgr;

    mainCamera_ = std::make_shared<BaseCamera>();
    mainCamera_->Initialize();

    if (cameraMgr_) {
        cameraMgr_->AddCamera(kMainCameraName, mainCamera_);
    }
}

void PlayCamera::Reset(const Vector3& playerPos, const Vector3& tangent) {
    Vector3 camPos = Math::Add(playerPos, Vector3{ 0.0f, kCameraUpHeight, 0.0f });
    Vector3 lookAtTarget = Math::Add(camPos, Math::Multiply(kCameraLookAhead, tangent));

    if (mainCamera_) {
        mainCamera_->SetPosition(camPos);
        mainCamera_->SetTarget(lookAtTarget);
        mainCamera_->SetRotation({ 0.2f, 0.0f, 0.0f });
        mainCamera_->Update();
    }

    prevLookTangent_ = tangent;
    cameraYawOffset_ = 0.0f;
    shakeTimer_ = 0;
}

void PlayCamera::TriggerShake() {
    shakeTimer_ = kShakeDuration;
}

void PlayCamera::Update(Input* input, const Vector3& playerPos, const Vector3& tangent, float currentDistance) {
    if (!input || !mainCamera_) return;

    // A/Dキーによるカメラ首振り制御
    if (input->Press(DIK_A)) {
        cameraYawOffset_ -= kCameraYawSpeed;
    } else if (input->Press(DIK_D)) {
        cameraYawOffset_ += kCameraYawSpeed;
    } else {
        // キーを離した際は正面（0.0f）に徐々に戻す
        if (cameraYawOffset_ > 0.0f) {
            cameraYawOffset_ -= kCameraYawReturnSpeed;
            if (cameraYawOffset_ < 0.0f) {
                cameraYawOffset_ = 0.0f;
            }
        } else if (cameraYawOffset_ < 0.0f) {
            cameraYawOffset_ += kCameraYawReturnSpeed;
            if (cameraYawOffset_ > 0.0f) {
                cameraYawOffset_ = 0.0f;
            }
        }
    }

    // 限界角にクランプ
    cameraYawOffset_ = std::clamp(cameraYawOffset_, -kCameraYawLimit, kCameraYawLimit);

    // 首振りを適用した注視方向ベクトルの計算
    float cosTheta = std::cos(cameraYawOffset_);
    float sinTheta = std::sin(cameraYawOffset_);
    Vector3 targetLookTangent;
    targetLookTangent.x = tangent.x * cosTheta + tangent.z * sinTheta;
    targetLookTangent.y = tangent.y; // Y軸回転なので上下は変更なし
    targetLookTangent.z = -tangent.x * sinTheta + tangent.z * cosTheta;
    targetLookTangent = Math::Normalize(targetLookTangent);

    // 前フレームの注視方向ベクトルと Lerp し、急激な首振りを平滑化する
    if (currentDistance <= 0.1f) {
        prevLookTangent_ = targetLookTangent;
    } else {
        prevLookTangent_.x = prevLookTangent_.x * (1.0f - kLookSmoothFactor) + targetLookTangent.x * kLookSmoothFactor;
        prevLookTangent_.y = prevLookTangent_.y * (1.0f - kLookSmoothFactor) + targetLookTangent.y * kLookSmoothFactor;
        prevLookTangent_.z = prevLookTangent_.z * (1.0f - kLookSmoothFactor) + targetLookTangent.z * kLookSmoothFactor;
        prevLookTangent_ = Math::Normalize(prevLookTangent_);
    }

    Vector3 camPos = Math::Add(playerPos, Vector3{ 0.0f, kCameraUpHeight, 0.0f });
    Vector3 lookAtTarget = Math::Add(camPos, Math::Multiply(kCameraLookAhead, prevLookTangent_));

    // カメラシェイクの適用
    if (shakeTimer_ > 0) {
        shakeTimer_--;
        float rx = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * kShakeIntensity;
        float ry = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * kShakeIntensity;
        float rz = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * kShakeIntensity;
        camPos = Math::Add(camPos, Vector3{ rx, ry, rz });
    }

    mainCamera_->SetPosition(camPos);
    mainCamera_->SetTarget(lookAtTarget);
    mainCamera_->Update();
}
