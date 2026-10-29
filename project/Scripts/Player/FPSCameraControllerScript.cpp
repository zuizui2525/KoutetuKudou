#include "Scripts/Player/FPSCameraControllerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
#include <cstdlib>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(FPSCameraControllerScript)

FPSCameraControllerScript::FPSCameraControllerScript()
    : ScriptComponent("FPSCameraControllerScript") {
}

void FPSCameraControllerScript::Awake() {
    cameraYawOffset_ = 0.0f;
    shakeTimer_ = 0;
    prevLookTangent_ = { 0.0f, 0.0f, 1.0f };
    deathCinematicProgress_ = 0.0f;
}

void FPSCameraControllerScript::Start() {
}

void FPSCameraControllerScript::OnUpdate() {
    if (!owner_) return;

    // プレイヤーの取得
    if (!targetObject_) {
        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    targetObject_ = obj.get();
                    targetUUID_ = obj->GetUUID();
                    break;
                }
            }
        }
    }

    if (!targetObject_) return;

    // プレイヤーの向きを取得
    Vector3 playerPos = targetObject_->GetPosition();
    Vector3 tangent = { 0.0f, 0.0f, 1.0f };
    if (auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>()) {
        tangent = playerScript->GetDirection();
    }

    UpdateFollow(playerPos, tangent);
}

namespace {
    constexpr float kEpsilon = 0.0001f;
    constexpr float kRandCenterOffset = 0.5f;     // 乱数の中心化オフセット
    constexpr float kPi = 3.1415926535f;
}

/// <summary>
/// 自機の位置と進行ベクトルに同期してFPSカメラの位置と注視回転を更新します。
/// 通常時は完全な一人称視点、死亡時は自機の側方を回り込んで正面から機体を捉えるシネマティックカメラワークを実行します。
/// </summary>
void FPSCameraControllerScript::UpdateFollow(const Vector3& targetPos, const Vector3& tangent) {
    auto* input = InputResource::GetInput();

    // 1. 死亡溶解状態の確認とシネマティック進行度の補間
    bool isMelting = false;
    if (targetObject_) {
        if (auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>()) {
            isMelting = playerScript->IsMelting();
        }
    }

    if (isMelting) {
        deathCinematicProgress_ += (1.0f - deathCinematicProgress_) * kDeathCameraLerpSpeed;
    } else {
        deathCinematicProgress_ = 0.0f;
    }

    // 2. A/Dキーによる首振り制御（通常時のみ）
    if (!isMelting && enableLean_ && input) {
        if (input->Press(DIK_A) || input->Press(DIK_LEFT)) {
            cameraYawOffset_ -= kCameraYawSpeed;
            if (cameraYawOffset_ < -kCameraYawLimit) cameraYawOffset_ = -kCameraYawLimit;
        } else if (input->Press(DIK_D) || input->Press(DIK_RIGHT)) {
            cameraYawOffset_ += kCameraYawSpeed;
            if (cameraYawOffset_ > kCameraYawLimit) cameraYawOffset_ = kCameraYawLimit;
        } else {
            // キー入力なしの時は徐々に正面に戻る
            if (cameraYawOffset_ > 0.0f) {
                cameraYawOffset_ -= kCameraYawReturnSpeed;
                if (cameraYawOffset_ < 0.0f) cameraYawOffset_ = 0.0f;
            } else if (cameraYawOffset_ < 0.0f) {
                cameraYawOffset_ += kCameraYawReturnSpeed;
                if (cameraYawOffset_ > 0.0f) cameraYawOffset_ = 0.0f;
            }
        }
    } else if (isMelting) {
        cameraYawOffset_ += (0.0f - cameraYawOffset_) * kDeathCameraLerpSpeed;
    }

    // 3. 注視方向ベクトルのスムージング
    prevLookTangent_.x += (tangent.x - prevLookTangent_.x) * kLookSmoothFactor;
    prevLookTangent_.y += (tangent.y - prevLookTangent_.y) * kLookSmoothFactor;
    prevLookTangent_.z += (tangent.z - prevLookTangent_.z) * kLookSmoothFactor;

    // 4. 通常時の一人称カメラ座標および注視点
    Vector3 normalCamPos = { targetPos.x, targetPos.y + kCameraUpHeight, targetPos.z };
    Vector3 normalLookAt = {
        normalCamPos.x + prevLookTangent_.x * kCameraLookAhead,
        normalCamPos.y + prevLookTangent_.y * kCameraLookAhead,
        normalCamPos.z + prevLookTangent_.z * kCameraLookAhead
    };

    Vector3 camPos = normalCamPos;
    Vector3 lookAtTarget = normalLookAt;

    // 5. 死亡シネマティックカメラの算出（円弧状に回り込んで正面から自機を注視）
    if (deathCinematicProgress_ > 0.001f) {
        float t = deathCinematicProgress_;

        // 進行方向の右方向ベクトル (XZ平面)
        Vector3 rightVec = { tangent.z, 0.0f, -tangent.x };
        float rightLen = std::sqrt(rightVec.x * rightVec.x + rightVec.z * rightVec.z);
        if (rightLen > kEpsilon) {
            rightVec.x /= rightLen;
            rightVec.z /= rightLen;
        } else {
            rightVec = { 1.0f, 0.0f, 0.0f };
        }

        // 正面カメラの目標座標 (自機前方 + 目線の高さ)
        Vector3 frontCamPos = {
            targetPos.x + tangent.x * kDeathCameraFrontDist,
            targetPos.y + kDeathCameraFrontHeight,
            targetPos.z + tangent.z * kDeathCameraFrontDist
        };

        // 自機の側方を円弧状に回り込むアーク膨らみ
        float sideArc = std::sin(t * kPi) * kDeathCameraSideSwing;

        camPos = {
            normalCamPos.x + (frontCamPos.x - normalCamPos.x) * t + rightVec.x * sideArc,
            normalCamPos.y + (frontCamPos.y - normalCamPos.y) * t,
            normalCamPos.z + (frontCamPos.z - normalCamPos.z) * t + rightVec.z * sideArc
        };

        // 死亡シネマティック時の注視目標点（自機中心）
        Vector3 deathLookAt = {
            targetPos.x,
            targetPos.y + kDeathTargetCenterHeight,
            targetPos.z
        };

        lookAtTarget = {
            normalLookAt.x + (deathLookAt.x - normalLookAt.x) * t,
            normalLookAt.y + (deathLookAt.y - normalLookAt.y) * t,
            normalLookAt.z + (deathLookAt.z - normalLookAt.z) * t
        };
    }

    // 6. 被弾シェイク演出
    if (shakeTimer_ > 0) {
        shakeTimer_--;
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * kShakeIntensity;
        float ry = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * kShakeIntensity;
        camPos.x += rx;
        camPos.y += ry;
    }

    owner_->SetPosition(camPos);

    // 7. 注視目標点への視線ベクトルから回転角 (Yaw, Pitch) を算出
    Vector3 lookDir = {
        lookAtTarget.x - camPos.x,
        lookAtTarget.y - camPos.y,
        lookAtTarget.z - camPos.z
    };
    float distXZ = std::sqrt(lookDir.x * lookDir.x + lookDir.z * lookDir.z);
    float yaw = std::atan2(lookDir.x, lookDir.z) + cameraYawOffset_ * (1.0f - deathCinematicProgress_);
    float pitch = -std::atan2(lookDir.y, distXZ > kEpsilon ? distXZ : kEpsilon);

    owner_->SetRotate({ pitch, yaw, 0.0f });
}

void FPSCameraControllerScript::TriggerShake() {
    shakeTimer_ = kShakeDuration;
}

void FPSCameraControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.3f, 0.8f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【FPSカメラ制御設定】");
    ImGui::Separator();

    ImGui::Checkbox("首振り入力有効##EnableLean", &enableLean_);
    ImGui::Text("現在の首振りオフセット: %.3f rad", cameraYawOffset_);
    if (ImGui::Button("テストシェイク発生##TriggerShake")) {
        TriggerShake();
    }
#endif
}
