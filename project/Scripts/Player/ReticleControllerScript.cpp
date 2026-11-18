#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(ReticleControllerScript)

ReticleControllerScript::ReticleControllerScript()
    : ScriptComponent("ReticleControllerScript") {
}

ReticleControllerScript::~ReticleControllerScript() {
    if (sInstance_ == this) {
        sInstance_ = nullptr;
    }
}

void ReticleControllerScript::Awake() {
    sInstance_ = this;
    hitMarkerTimer_ = 0;
    isHitCritical_ = false;
}

void ReticleControllerScript::TriggerHitMarker(bool isCritical) {
    if (sInstance_) {
        sInstance_->hitMarkerTimer_ = kDefaultHitMarkerDuration;
        sInstance_->isHitCritical_ = isCritical;
    }
}

void ReticleControllerScript::EnsureHitMarkerVisuals() {
    for (int i = 0; i < 4; ++i) {
        if (!hitMarkerLines_[i]) {
            hitMarkerLines_[i] = std::make_unique<Line2DObject>();
            hitMarkerLines_[i]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(hitMarkerLines_[i].get());
        }
    }
}

/// <summary>
/// 走行戦闘中（APEXスタイル）はレティクルを画面中央に完全固定し、
/// カーソル追従モード時はマウス座標に追従させます。
/// </summary>
void ReticleControllerScript::OnUpdate() {
    if (hitMarkerTimer_ > 0) {
        hitMarkerTimer_--;
    }

    if (!owner_) return;

    // エリアクリア演出中はレティクルを非表示化（三人称見送りシネマティックに集中）
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing()) {
            owner_->SetVisible(false);
            return;
        }
    }

    if (isLockedToCenter_) {
        // APEXスタイル: 常に画面の中心（クライアント解像度の中央）へ固定配置
        constexpr float kClientCenterX = static_cast<float>(WindowApp::kClientWidth) * 0.5f;
        constexpr float kClientCenterY = static_cast<float>(WindowApp::kClientHeight) * 0.5f;
        owner_->SetPosition({ kClientCenterX - halfSize_, kClientCenterY - halfSize_, 0.0f });
        return;
    }

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        return;
    }

    float scaleX = static_cast<float>(WindowApp::kClientWidth) / viewSize.x;
    float scaleY = static_cast<float>(WindowApp::kClientHeight) / viewSize.y;
    Vector2 scaledMousePos = { mousePos.x * scaleX, mousePos.y * scaleY };

    owner_->SetPosition({ scaledMousePos.x - halfSize_, scaledMousePos.y - halfSize_, 0.0f });
}

/// <summary>
/// 敵命中時にレティクル中央に「×」印のヒットマーカーを描画します（2Dレンダリングパス）。
/// </summary>
void ReticleControllerScript::Draw2D() {
    if (hitMarkerTimer_ <= 0) return;

    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing()) return;
    }

    EnsureHitMarkerVisuals();

    Vector2 center;
    if (isLockedToCenter_) {
        center.x = static_cast<float>(WindowApp::kClientWidth) * 0.5f;
        center.y = static_cast<float>(WindowApp::kClientHeight) * 0.5f;
    } else {
        if (owner_) {
            center.x = owner_->GetPosition().x + halfSize_;
            center.y = owner_->GetPosition().y + halfSize_;
        } else {
            center.x = static_cast<float>(WindowApp::kClientWidth) * 0.5f;
            center.y = static_cast<float>(WindowApp::kClientHeight) * 0.5f;
        }
    }

    Vector4 color = isHitCritical_ ? kCriticalHitColor : kNormalHitColor;

    // 4本の斜め腕（×印: 0:右上, 1:左上, 2:右下, 3:左下）
    const Vector2 dirs[4] = {
        {  1.0f, -1.0f },
        { -1.0f, -1.0f },
        {  1.0f,  1.0f },
        { -1.0f,  1.0f }
    };

    for (int i = 0; i < 4; ++i) {
        if (!hitMarkerLines_[i]) continue;
        Vector2 start = { center.x + dirs[i].x * kHitMarkerGap, center.y + dirs[i].y * kHitMarkerGap };
        Vector2 end   = { center.x + dirs[i].x * (kHitMarkerGap + kHitMarkerLength), center.y + dirs[i].y * (kHitMarkerGap + kHitMarkerLength) };
        hitMarkerLines_[i]->SetPoints(start, end);
        hitMarkerLines_[i]->SetThickness(kHitMarkerThickness);
        if (hitMarkerLines_[i]->GetMaterialData()) {
            hitMarkerLines_[i]->GetMaterialData()->color = color;
        }
        hitMarkerLines_[i]->Update();
        hitMarkerLines_[i]->Draw("white", true);
    }
}

void ReticleControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.9f, 0.4f, 0.4f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【レティクルUI設定】");
    ImGui::Separator();

    ImGui::Checkbox("画面中央固定 (APEXスタイル)##LockToCenter", &isLockedToCenter_);

    float currentSize = reticleSize_;
    if (ImGui::DragFloat("レティクルサイズ##ReticleSize", &currentSize, 1.0f, 16.0f, 512.0f, "%.0f")) {
        SetReticleSize(currentSize);
    }
#endif
}
