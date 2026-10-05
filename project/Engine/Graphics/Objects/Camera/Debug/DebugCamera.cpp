#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include <algorithm>
#include "imgui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Debug/GameViewWindow.h"

namespace {
    constexpr float kMaxPitch = 1.57f;
    constexpr float kWheelSensitivity = 0.005f;
    constexpr float kFastMoveMultiplier = 2.5f;
}

void DebugCamera::Initialize() {
    BaseCamera::Initialize();
    hwnd_ = EngineResource::GetEngine()->GetWindow()->GetHWND();
    assert(hwnd_ != nullptr);
}

void DebugCamera::Update(Input* input) {
#ifdef _USEIMGUI
    if (!hwnd_ || !isActive_) return;

    // Ctrl キー（左または右）が押されているか
    bool isCtrlOperating = input->Press(DIK_LCONTROL) || input->Press(DIK_RCONTROL);
    if (input->Trigger(DIK_LCONTROL) || input->Trigger(DIK_RCONTROL)) {
        isFirstOperatingFrame_ = true;
    }

    // 右クリックによる操作判定（開始時にゲームビュー上である場合のみ有効）
    if (input->MouseTrigger(1)) {
        if (GameViewWindow::IsMouseOnGameView()) {
            isRightClickOperating_ = true;
            isFirstOperatingFrame_ = true;
            GetCursorPos(&startCursorPos_);
        }
    }

    if (!input->MousePress(1) && isRightClickOperating_) {
        // 右クリックを離した瞬間に元のマウス位置へ復元
        SetCursorPos(startCursorPos_.x, startCursorPos_.y);
        isRightClickOperating_ = false;
        SetCursorVisible(true);
    }

    bool isOperating = isCtrlOperating || isRightClickOperating_;

    if (isOperating) {
        // --- 1. カーソル制御と中央固定 ---
        int centerX = WindowApp::kClientWidth / 2;
        int centerY = WindowApp::kClientHeight / 2;
        POINT center = { centerX, centerY };

        // 操作中はカーソルを隠す
        SetCursorVisible(false);

        if (isFirstOperatingFrame_) {
            // ★操作開始の最初のフレームはマウスを中央にワープさせるのみで、回転は行わない
            // これにより、クリック位置と中央位置の差分による急激な視線飛びを完全に防止
            ClientToScreen(hwnd_, &center);
            SetCursorPos(center.x, center.y);
            isFirstOperatingFrame_ = false;
        } else {
            POINT currentPos;
            GetCursorPos(&currentPos);
            ScreenToClient(hwnd_, &currentPos);

            // 中心からの移動量を取得
            int dx = currentPos.x - center.x;
            int dy = currentPos.y - center.y;

            // マウスを中央に戻す
            ClientToScreen(hwnd_, &center);
            SetCursorPos(center.x, center.y);

            // --- 2. 回転処理 ---
            transform_.rotate.x += static_cast<float>(dy) * rotateSpeed_;
            transform_.rotate.y += static_cast<float>(dx) * rotateSpeed_;

            // 垂直方向の回転制限
            transform_.rotate.x = std::clamp(transform_.rotate.x, -kMaxPitch, kMaxPitch);
        }

        // --- 3. 移動処理 (WASD + Space/LShift + E/Q) ---
        Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(transform_.rotate.x, transform_.rotate.y, transform_.rotate.z);
        Vector3 forward = Math::TransformNormal({ 0, 0, 1 }, rotateMatrix);
        Vector3 right = Math::TransformNormal({ 1, 0, 0 }, rotateMatrix);
        Vector3 up = { 0, 1, 0 };

        Vector3 move = { 0, 0, 0 };
        if (input->Press(DIK_W)) move = move + forward;
        if (input->Press(DIK_S)) move = move - forward;
        if (input->Press(DIK_D)) move = move + right;
        if (input->Press(DIK_A)) move = move - right;
        if (input->Press(DIK_SPACE) || input->Press(DIK_E)) move = move + up;
        if (input->Press(DIK_LSHIFT) || input->Press(DIK_Q)) move = move - up;

        if (Math::Length(move) > 0) {
            float speed = moveSpeed_;
            // 右クリック操作中にShiftが押されていたら高速移動
            if (isRightClickOperating_ && input->Press(DIK_LSHIFT)) {
                speed *= kFastMoveMultiplier;
            }
            move = Math::Normalize(move) * speed;
            transform_.translate = transform_.translate + move;
        }
    } else {
        // キー・右クリックが離されているときはカーソルを表示する
        SetCursorVisible(true);

        // ゲーム画面上のホイールで前後にズーム
        if (GameViewWindow::IsMouseOnGameView()) {
            float wheel = input->GetMouseWheel();
            if (std::abs(wheel) > 0.0f) {
                Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(transform_.rotate.x, transform_.rotate.y, transform_.rotate.z);
                Vector3 forward = Math::TransformNormal({ 0, 0, 1 }, rotateMatrix);
                transform_.translate = transform_.translate + forward * (wheel * kWheelSensitivity);
            }
        }
    }

    BaseCamera::Update();
#else
    (void)input;
#endif
}

void DebugCamera::SetCursorVisible(bool isVisible) {
    if (isVisible && !isCursorVisible_) {
        while (ShowCursor(TRUE) < 0);
        isCursorVisible_ = true;
    } else if (!isVisible && isCursorVisible_) {
        while (ShowCursor(FALSE) >= 0);
        isCursorVisible_ = false;
    }
}

void DebugCamera::SetActive(bool active) {
    isActive_ = active;
    if (!isActive_) {
        if (isRightClickOperating_) {
            SetCursorPos(startCursorPos_.x, startCursorPos_.y);
            isRightClickOperating_ = false;
        }
        DebugCamera::SetCursorVisible(true);
    }
}

