#include "Scripts/Director/TitleDirectorScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/3d/Skybox/Skybox.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    const std::string kTitleSkyboxTextureKey = "white";
    constexpr Vector4 kTitleSkyboxColor = { 0.02f, 0.02f, 0.03f, 1.0f }; // 重厚なダークブラック背景
}

TitleDirectorScript::TitleDirectorScript()
    : ScriptComponent("TitleDirectorScript") {
}

TitleDirectorScript::~TitleDirectorScript() = default;

void TitleDirectorScript::Start() {
    // ポストプロセスの演出設定
    if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
        postProcess->SetDepthOutlineActive(true);
        postProcess->SetGrayscaleActive(false);
        postProcess->SetVignetteActive(true);
    }

    // Skybox の生成と初期化
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize();
    skybox_->SetColor(kTitleSkyboxColor);
}

void TitleDirectorScript::OnUpdate() {
    // スペースキー押下でゲームシーンへ遷移
    auto input = InputResource::GetInput();
    if (input && input->Trigger(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene(nextSceneName_);
        return;
    }

    // 背景 Skybox の更新
    if (skybox_) {
        skybox_->Update();
    }
}

void TitleDirectorScript::Draw() {
    // 背景 Skybox の 3D 描画
    if (skybox_) {
        skybox_->Draw(kTitleSkyboxTextureKey);
    }
}

void TitleDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[128];
    strncpy_s(buffer, nextSceneName_.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText("Next Scene", buffer, sizeof(buffer))) {
        nextSceneName_ = buffer;
    }
    ImGui::Text("Press SPACE to transit to: %s", nextSceneName_.c_str());
#endif
}

REGISTER_SCRIPT(TitleDirectorScript);
