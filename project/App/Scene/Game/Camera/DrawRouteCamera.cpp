#include "App/Scene/Game/Camera/DrawRouteCamera.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Input/Input.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include <algorithm>

DrawRouteCamera::DrawRouteCamera() = default;
DrawRouteCamera::~DrawRouteCamera() = default;

void DrawRouteCamera::Initialize(CameraManager* cameraMgr, float startZ) {
    cameraMgr_ = cameraMgr;

    // ズームカメラの生成
    cameraZoom_ = std::make_shared<BaseCamera>();
    cameraZoom_->Initialize();

    targetZoom_ = { 0.0f, 0.0f, startZ };
    destinationZoom_ = targetZoom_;
    zoomFactor_ = 1.0f;
    targetZoomFactor_ = 1.0f;
    isEasing_ = false;

    cameraZoom_->SetPosition(Math::Add(targetZoom_, kZoomCameraOffset));
    cameraZoom_->SetTarget(targetZoom_);

    if (cameraMgr_) {
        cameraMgr_->AddCamera("Zoom", cameraZoom_);
    }

    // 視野可視化用枠線の初期化（緑のライン）
    zoomFrameLines_.clear();
    for (int i = 0; i < 4; ++i) {
        auto line = std::make_unique<LineObject>();
        line->Initialize(0);
        line->SetThickness(kFrameLineThickness);
        line->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        zoomFrameLines_.push_back(std::move(line));
    }
}

void DrawRouteCamera::Update(Input* input, float startZ, float goalZ) {
    if (!input) return;

    Vector2 mousePos = GameViewWindow::GetMousePosition();
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);

    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    float vpWidth = clientW * 0.3f;

    // 1. 右ドラッグによる自由スクロール
    if (input->MousePress(1) && scaledMousePos.x > vpWidth) {
        float dx = input->GetMouseDeltaX();
        float dy = input->GetMouseDeltaY();

        destinationZoom_.x += dx * kZoomScrollSpeed;
        destinationZoom_.z -= dy * kZoomScrollSpeed;

        destinationZoom_.x = std::clamp(destinationZoom_.x, -kMapLimitX, kMapLimitX);
        destinationZoom_.z = std::clamp(destinationZoom_.z, startZ, goalZ);
        
        isEasing_ = false; // 手動スクロール時はイージングを解除
    }

    // 2. マウスホイールによるズームイン・アウト (右画面にマウスがある場合のみ)
    if (scaledMousePos.x > vpWidth) {
        float wheel = input->GetMouseWheel();
        if (wheel != 0.0f) {
            targetZoomFactor_ -= wheel * kZoomSensitivity;
            targetZoomFactor_ = std::clamp(targetZoomFactor_, kMinZoom, kMaxZoom);
            isEasing_ = false; // 手動ズーム時はイージングを解除
        }
    }

    // 3. マウスホイールクリック（中央クリック）で中央＆標準ズームにイージングで戻る
    if (scaledMousePos.x > vpWidth && input->MouseTrigger(2)) {
        isEasing_ = true;
        destinationZoom_.x = 0.0f; // X座標を中央に戻す (Z座標は現在のカメラ注視Zを維持)
        targetZoomFactor_ = 1.0f;  // 標準ズーム（1.0f）に戻す
    }

    // 4. イージング・ Lerp 補間の適用
    float currentLerpRate = kLerpRate;
    if (input->MousePress(0)) {
        // 左クリックドラッグ（手書き描画）中は操作追従の遅れを防ぐため即時に適用する
        currentLerpRate = 1.0f;
    }

    targetZoom_.x += (destinationZoom_.x - targetZoom_.x) * currentLerpRate;
    targetZoom_.y += (destinationZoom_.y - targetZoom_.y) * currentLerpRate;
    targetZoom_.z += (destinationZoom_.z - targetZoom_.z) * currentLerpRate;

    zoomFactor_ += (targetZoomFactor_ - zoomFactor_) * kZoomLerpRate;

    // 戻りきった際のイージング終了判定
    if (isEasing_) {
        float distToDestX = std::abs(targetZoom_.x - destinationZoom_.x);
        float distToDestZoom = std::abs(zoomFactor_ - targetZoomFactor_);
        if (distToDestX < 0.01f && distToDestZoom < 0.01f) {
            targetZoom_.x = destinationZoom_.x;
            zoomFactor_ = targetZoomFactor_;
            isEasing_ = false;
        }
    }

    // 5. ズームカメラの位置と注視点を反映
    if (cameraZoom_) {
        cameraZoom_->SetPosition(Math::Add(targetZoom_, Math::Multiply(zoomFactor_, kZoomCameraOffset)));
        cameraZoom_->SetTarget(targetZoom_);
        cameraZoom_->Update();
    }
}

void DrawRouteCamera::SetTargetZoom(const Vector3& target) {
    targetZoom_ = target;
    destinationZoom_ = target;
    // 即座に反映
    if (cameraZoom_) {
        cameraZoom_->SetPosition(Math::Add(targetZoom_, Math::Multiply(zoomFactor_, kZoomCameraOffset)));
        cameraZoom_->SetTarget(targetZoom_);
        cameraZoom_->Update();
    }
}

void DrawRouteCamera::SetDestinationZoom(const Vector3& target) {
    destinationZoom_ = target;
}
