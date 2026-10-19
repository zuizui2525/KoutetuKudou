#include "Scripts/Director/GameOverDirectorScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

GameOverDirectorScript::GameOverDirectorScript()
    : ScriptComponent("GameOverDirectorScript") {
}

GameOverDirectorScript::~GameOverDirectorScript() = default;

void GameOverDirectorScript::Start() {
    // ポストプロセスの演出設定
    if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
        postProcess->SetVignetteActive(true);
        postProcess->SetTVNoiseActive(true);
        postProcess->SetTVNoiseStrength(tvNoiseStrength_);
        postProcess->SetClearColorMode(PostClearColorMode::Red);
    }
}

void GameOverDirectorScript::OnUpdate() {
    // スペースキー押下でタイトルシーンへ遷移
    auto input = InputResource::GetInput();
    if (input && input->Trigger(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene(nextSceneName_);
        return;
    }
}

void GameOverDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[128];
    strncpy_s(buffer, nextSceneName_.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText("Next Scene", buffer, sizeof(buffer))) {
        nextSceneName_ = buffer;
    }
    if (ImGui::SliderFloat("TV Noise", &tvNoiseStrength_, 0.0f, 1.0f)) {
        if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
            postProcess->SetTVNoiseStrength(tvNoiseStrength_);
        }
    }
#endif
}

REGISTER_SCRIPT(GameOverDirectorScript);
