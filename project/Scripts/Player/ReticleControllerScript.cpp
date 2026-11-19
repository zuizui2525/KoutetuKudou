#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include <algorithm>
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
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
    hitConfirmRing_.reset();
    for (int i = 0; i < 4; ++i) {
        hitMarkerLines_[i].reset();
    }
}

void ReticleControllerScript::Awake() {
    sInstance_ = this;
    hitMarkerTimer_ = 0;
    isHitCritical_ = false;
}

void ReticleControllerScript::Start() {
    // 初期レティクルサイズ（52px）をSpriteRendererへ確実に同期
    SetReticleSize(reticleSize_);
}

void ReticleControllerScript::SetReticleSize(float size) {
    constexpr float kHalf = 0.5f;
    reticleSize_ = size;
    halfSize_ = size * kHalf;
    if (owner_) {
        if (auto* spriteComp = owner_->GetComponent<SpriteRendererComponent>()) {
            spriteComp->SetSize({ reticleSize_, reticleSize_ });
        }
    }
}

void ReticleControllerScript::TriggerHitMarker(bool isCritical) {
    if (sInstance_) {
        sInstance_->hitMarkerTimer_ = kDefaultHitMarkerDuration;
        sInstance_->isHitCritical_ = isCritical;
    }
}

void ReticleControllerScript::EnsureHitMarkerVisuals() {
    if (!hitConfirmRing_) {
        hitConfirmRing_ = std::make_unique<Ring2DObject>();
        hitConfirmRing_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hitConfirmRing_.get());
    }
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
/// 敵命中時にレティクル外周へ弾けるヒット確認リングおよび4方向ワイドティックを描画します（2Dレンダリングパス）。
/// レティクル中心や敵本体を一切隠さず、周辺視野で瞬時に命中とクリティカルを視認できます。
/// </summary>
void ReticleControllerScript::Draw2D() {
    if (hitMarkerTimer_ <= 0) return;

    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing() || phaseCtrl->IsLevelUpChoosing()) return;
    }

    EnsureHitMarkerVisuals();

    constexpr float kHalf = 0.5f;
    constexpr float kInvSqrt2 = 0.70710678f;
    constexpr float kMinInnerRadius = 1.0f;

    Vector2 center;
    if (isLockedToCenter_) {
        center.x = static_cast<float>(WindowApp::kClientWidth) * kHalf;
        center.y = static_cast<float>(WindowApp::kClientHeight) * kHalf;
    } else {
        if (owner_) {
            center.x = owner_->GetPosition().x + halfSize_;
            center.y = owner_->GetPosition().y + halfSize_;
        } else {
            center.x = static_cast<float>(WindowApp::kClientWidth) * kHalf;
            center.y = static_cast<float>(WindowApp::kClientHeight) * kHalf;
        }
    }

    // 進行度 (0.0: 命中直後 → 1.0: 消滅) とイーズアウト曲線
    float progress = 1.0f - (static_cast<float>(hitMarkerTimer_) / static_cast<float>(kDefaultHitMarkerDuration));
    progress = std::clamp(progress, 0.0f, 1.0f);
    float easeOut = 1.0f - (1.0f - progress) * (1.0f - progress);
    float alpha = 1.0f - progress * progress;

    Vector4 baseColor = isHitCritical_ ? kCriticalHitColor : kNormalHitColor;
    Vector4 dynamicColor = { baseColor.x, baseColor.y, baseColor.z, baseColor.w * alpha };

    // 1. 外周へパッと弾け広がるヒット確認リング（狙っている敵を一切塞がない）
    if (hitConfirmRing_) {
        float startR = isHitCritical_ ? kCriticalRingStartRadius : kHitRingStartRadius;
        float endR = isHitCritical_ ? kCriticalRingEndRadius : kHitRingEndRadius;
        float thickness = isHitCritical_ ? kCriticalRingThickness : kHitRingThickness;
        float currentR = startR + (endR - startR) * easeOut;

        hitConfirmRing_->SetRadii(currentR, (std::max)(kMinInnerRadius, currentR - thickness));
        if (hitConfirmRing_->GetMaterialData()) {
            hitConfirmRing_->GetMaterialData()->color = dynamicColor;
        }
        hitConfirmRing_->SetPosition({ center.x - currentR, center.y - currentR, 0.0f });
        hitConfirmRing_->Update();
        hitConfirmRing_->Draw("white", true);
    }

    // 2. レティクル外周の4方向ワイドティック（命中瞬間に外へ弾けながらフェード）
    float currentGap = kHitTickBaseGap + kHitTickPopDist * easeOut;
    float tickLen = isHitCritical_ ? kCriticalTickLength : kHitTickLength;
    float tickThickness = isHitCritical_ ? kCriticalTickThickness : kHitTickThickness;

    const Vector2 dirs[4] = {
        {  kInvSqrt2, -kInvSqrt2 },
        { -kInvSqrt2, -kInvSqrt2 },
        {  kInvSqrt2,  kInvSqrt2 },
        { -kInvSqrt2,  kInvSqrt2 }
    };

    for (int i = 0; i < 4; ++i) {
        if (!hitMarkerLines_[i]) continue;
        Vector2 start = { center.x + dirs[i].x * currentGap, center.y + dirs[i].y * currentGap };
        Vector2 end   = { center.x + dirs[i].x * (currentGap + tickLen), center.y + dirs[i].y * (currentGap + tickLen) };
        hitMarkerLines_[i]->SetPoints(start, end);
        hitMarkerLines_[i]->SetThickness(tickThickness);
        if (hitMarkerLines_[i]->GetMaterialData()) {
            hitMarkerLines_[i]->GetMaterialData()->color = dynamicColor;
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
