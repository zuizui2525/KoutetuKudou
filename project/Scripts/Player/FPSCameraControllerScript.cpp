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
        float rx = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * kShakeIntensity;
        float ry = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * kShakeIntensity;
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
    float pitch = -std::atan2(prevLookTangent_.y, distXZ > 0.0001f ? distXZ : 0.0001f);

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
