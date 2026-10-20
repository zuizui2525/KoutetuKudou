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
    constexpr float kRandCenterOffset = 0.5f; // 乱数の中心化オフセット
}

/// <summary>
/// 自機の位置と進行ベクトルに同期してFPSカメラの位置と注視回転を更新します。
/// 
/// 【制御アルゴリズム】
/// 1. 首振り制御: A/Dキーまたは左右矢印キーでカメラの水平回転オフセット（cameraYawOffset_）を増減。
///    キー入力がない場合は正面へ復帰（kCameraYawReturnSpeed）。
/// 2. カメラ位置: 自機座標から目の高さ（kCameraUpHeight）だけ上方にオフセット。
/// 3. シェイク演出: shakeTimer_ 継続中はランダムな微小変位をカメラ座標に加算。
/// 4. 視線平滑化: 急激な視線変化を抑えるため、進行方向ベクトルを線形補間（kLookSmoothFactor）で平滑化。
/// 5. 姿勢回転: 平滑化接線と首振りオフセットから Yaw・Pitch 角を求めてカメラ回転を更新。
/// </summary>
void FPSCameraControllerScript::UpdateFollow(const Vector3& targetPos, const Vector3& tangent) {
    auto* input = InputResource::GetInput();

    // 1. A/Dキーによる首振り制御
    if (enableLean_ && input) {
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
    }

    // 2. カメラ位置の設定（目の高さ）
    Vector3 camPos = {
        targetPos.x,
        targetPos.y + kCameraUpHeight,
        targetPos.z
    };

    // 3. 被弾シェイク演出
    if (shakeTimer_ > 0) {
        shakeTimer_--;
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * kShakeIntensity;
        float ry = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * kShakeIntensity;
        camPos.x += rx;
        camPos.y += ry;
    }

    owner_->SetPosition(camPos);

    // 4. 注視方向のスムージングと回転算出
    prevLookTangent_.x += (tangent.x - prevLookTangent_.x) * kLookSmoothFactor;
    prevLookTangent_.y += (tangent.y - prevLookTangent_.y) * kLookSmoothFactor;
    prevLookTangent_.z += (tangent.z - prevLookTangent_.z) * kLookSmoothFactor;

    float yaw = std::atan2(prevLookTangent_.x, prevLookTangent_.z) + cameraYawOffset_;
    float distXZ = std::sqrt(prevLookTangent_.x * prevLookTangent_.x + prevLookTangent_.z * prevLookTangent_.z);
    float pitch = -std::atan2(prevLookTangent_.y, distXZ > kEpsilon ? distXZ : kEpsilon);

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
