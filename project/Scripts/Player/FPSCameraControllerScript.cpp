#include "Scripts/Player/FPSCameraControllerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(FPSCameraControllerScript)

FPSCameraControllerScript::FPSCameraControllerScript()
    : ScriptComponent("FPSCameraControllerScript") {
}

FPSCameraControllerScript::~FPSCameraControllerScript() {
    UpdateCursorLock(false);
}

void FPSCameraControllerScript::Awake() {
    cameraYawOffset_ = 0.0f;
    cameraPitchOffset_ = 0.0f;
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
    constexpr float kTwoPi = 6.283185307f;
    constexpr float kNormalizedCenterScale = 0.5f; // 画面半分スケール係数
}

/// <summary>
/// 自機の位置と進行ベクトルに同期してFPSカメラの位置と注視回転を更新します。
/// 【APEXスタイル】マウスの相対移動量 (Delta) に応じた360度ダイレクト視線旋回、
/// 通常時の一人称視点、死亡時の自機正面回り込みシネマティックカメラワークを実行します。
/// </summary>
void FPSCameraControllerScript::UpdateFollow(const Vector3& targetPos, const Vector3& tangent) {
    auto* input = InputResource::GetInput();

    // 1. プレイヤーの状態確認（死亡溶解中か、走行戦闘中か）
    bool isMelting = false;
    bool isAutoMoving = false;
    if (targetObject_) {
        if (auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>()) {
            isMelting = playerScript->IsMelting();
            isAutoMoving = playerScript->IsAutoMoving();
        }
    }

    if (isMelting) {
        deathCinematicProgress_ += (1.0f - deathCinematicProgress_) * kDeathCameraLerpSpeed;
    } else {
        deathCinematicProgress_ = 0.0f;
    }

    // 2. マウスおよびキー入力による首振り制御（走行戦闘中のみ有効）
    // ルート手書き中（DrawRouteフェーズ）や発進待機中は首振りを無効化し、線引き操作に集中できるようにする
    bool isPlaying = (!isMelting && isAutoMoving && useDirectAim_);
    UpdateCursorLock(isPlaying);

    if (!isMelting && isAutoMoving) {
        if (enableMouseTracking_ && input) {
            if (useDirectAim_) {
                // 【APEXスタイル】マウスの移動デルタ（Delta X, Delta Y）によるダイレクト視点旋回
                float deltaX = input->GetMouseDeltaX();
                float deltaY = input->GetMouseDeltaY();

                // 水平旋回 (Yaw: 左右360度制限なし、角度を [-π, π] に正規化)
                cameraYawOffset_ += deltaX * mouseSensitivity_;
                while (cameraYawOffset_ > kPi) cameraYawOffset_ -= kTwoPi;
                while (cameraYawOffset_ < -kPi) cameraYawOffset_ += kTwoPi;

                // 垂直旋回 (Pitch: 上下最大±75度で裏返り防止)
                // マウス手前引き (deltaY > 0) で下向き見下ろし、前へ押し出し (deltaY < 0) で上向き見上げ
                cameraPitchOffset_ += deltaY * mouseSensitivity_;
                cameraPitchOffset_ = std::clamp(cameraPitchOffset_, -kMaxPitchLimit, kMaxPitchLimit);
            } else {
                // 画面端カーソル追従モード
                Vector2 viewSize = GameViewWindow::GetGameViewSize();
                Vector2 mousePos = GameViewWindow::GetMousePosition();

                if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
                    float halfW = viewSize.x * kNormalizedCenterScale;
                    float halfH = viewSize.y * kNormalizedCenterScale;
                    float nx = std::clamp((mousePos.x - halfW) / halfW, -1.0f, 1.0f);
                    float ny = std::clamp((mousePos.y - halfH) / halfH, -1.0f, 1.0f);

                    float signX = (nx >= 0.0f) ? 1.0f : -1.0f;
                    float curvedX = signX * std::pow(std::abs(nx), kAimCurveExponent);
                    float targetYaw = curvedX * kCameraYawLimitHalf;

                    float signY = (ny >= 0.0f) ? 1.0f : -1.0f;
                    float curvedY = signY * std::pow(std::abs(ny), kAimCurveExponent);
                    float targetPitch = curvedY * kCameraPitchLimit;

                    cameraYawOffset_ += (targetYaw - cameraYawOffset_) * kMouseTrackSmoothSpeed;
                    cameraPitchOffset_ += (targetPitch - cameraPitchOffset_) * kMouseTrackSmoothSpeed;
                }
            }
        }

        // A/Dキーによる微調整（キー入力有効時）
        if (enableLean_ && input) {
            if (input->Press(DIK_A) || input->Press(DIK_LEFT)) {
                cameraYawOffset_ -= kCameraYawSpeed;
            } else if (input->Press(DIK_D) || input->Press(DIK_RIGHT)) {
                cameraYawOffset_ += kCameraYawSpeed;
            }
            while (cameraYawOffset_ > kPi) cameraYawOffset_ -= kTwoPi;
            while (cameraYawOffset_ < -kPi) cameraYawOffset_ += kTwoPi;
        }
    } else {
        // 走行前（線引き中）、待機中、または死亡演出中は正面へスムーズに戻す
        cameraYawOffset_ += (0.0f - cameraYawOffset_) * kCameraYawReturnSpeed;
        cameraPitchOffset_ += (0.0f - cameraPitchOffset_) * kCameraYawReturnSpeed;
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
    float pitch = -std::atan2(lookDir.y, distXZ > kEpsilon ? distXZ : kEpsilon) + cameraPitchOffset_ * (1.0f - deathCinematicProgress_);

    owner_->SetRotate({ pitch, yaw, 0.0f });
}

void FPSCameraControllerScript::TriggerShake() {
    shakeTimer_ = kShakeDuration;
}

void FPSCameraControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.3f, 0.8f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【FPSカメラ制御設定 (APEXスタイル)】");
    ImGui::Separator();

    ImGui::Checkbox("ダイレクトエイム (APEXスタイル)##UseDirectAim", &useDirectAim_);
    ImGui::SliderFloat("マウス感度##MouseSensitivity", &mouseSensitivity_, kMinMouseSensitivity, kMaxMouseSensitivity, "%.4f");
    ImGui::Checkbox("マウス視線連動##EnableMouseTrack", &enableMouseTracking_);
    ImGui::Checkbox("キー首振り入力有効##EnableLean", &enableLean_);
    ImGui::Text("現在の首振りYaw: %.3f rad", cameraYawOffset_);
    ImGui::Text("現在の首振りPitch: %.3f rad", cameraPitchOffset_);
    if (ImGui::Button("テストシェイク発生##TriggerShake")) {
        TriggerShake();
    }
#endif
}

void FPSCameraControllerScript::UpdateCursorLock(bool shouldLock) {
    if (shouldLock) {
        // カーソル非表示化（重複呼び出し防止）
        if (!isCursorLocked_) {
            ShowCursor(FALSE);
            isCursorLocked_ = true;
        }

#ifdef _USEIMGUI
        // ImGui によるカーソル再描画を確実に防ぐため、毎フレーム非表示カーソルを指定
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
#endif

        // 自ウィンドウがアクティブな場合のみマウスカーソルを画面中央へ固定
        HWND activeHwnd = GetForegroundWindow();
        HWND appHwnd = nullptr;
        if (auto* engine = Zuizui::GetInstance()) {
            if (auto* win = engine->GetWindow()) {
                appHwnd = win->GetHWND();
            }
        }

        if (activeHwnd && appHwnd && activeHwnd == appHwnd) {
            Vector2 viewPosMin = GameViewWindow::GetGameViewPosMin();
            Vector2 viewSize = GameViewWindow::GetGameViewSize();
            if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
                constexpr float kCenterRatio = 0.5f;
                POINT centerPt{};
#ifdef _USEIMGUI
                centerPt.x = static_cast<LONG>(viewPosMin.x + viewSize.x * kCenterRatio);
                centerPt.y = static_cast<LONG>(viewPosMin.y + viewSize.y * kCenterRatio);
#else
                centerPt.x = static_cast<LONG>(viewSize.x * kCenterRatio);
                centerPt.y = static_cast<LONG>(viewSize.y * kCenterRatio);
                ClientToScreen(appHwnd, &centerPt);
#endif
                SetCursorPos(centerPt.x, centerPt.y);
            }
        }
    } else {
        // カーソル再表示・固定解除
        if (isCursorLocked_) {
            ShowCursor(TRUE);
            isCursorLocked_ = false;
        }
    }
}
