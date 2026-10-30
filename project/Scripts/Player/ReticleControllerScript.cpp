#include "Scripts/Player/ReticleControllerScript.h"
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

/// <summary>
/// 走行戦闘中（APEXスタイル）はレティクルを画面中央に完全固定し、
/// カーソル追従モード時はマウス座標に追従させます。
/// </summary>
void ReticleControllerScript::OnUpdate() {
    if (!owner_) return;

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
