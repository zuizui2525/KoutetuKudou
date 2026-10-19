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

void ReticleControllerScript::OnUpdate() {
    if (!owner_) return;

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

    float currentSize = reticleSize_;
    if (ImGui::DragFloat("レティクルサイズ##ReticleSize", &currentSize, 1.0f, 16.0f, 512.0f, "%.0f")) {
        SetReticleSize(currentSize);
    }
#endif
}
