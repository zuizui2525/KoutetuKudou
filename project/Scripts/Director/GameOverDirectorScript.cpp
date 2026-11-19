#include "Scripts/Director/GameOverDirectorScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    constexpr size_t kImGuiBufferLength = 128;
    constexpr int kLeftMouseButton = 0;
}

GameOverDirectorScript::GameOverDirectorScript()
    : ScriptComponent("GameOverDirectorScript") {
}

GameOverDirectorScript::~GameOverDirectorScript() = default;

/// <summary>
/// ゲームオーバーシーン開始時に、赤色クリアカラー、ビネット、TVノイズのポストプロセス演出を有効化します。
/// </summary>
void GameOverDirectorScript::Start() {
    // ポストプロセスの演出設定
    if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
        postProcess->SetVignetteActive(true);
        postProcess->SetTVNoiseActive(true);
        postProcess->SetTVNoiseStrength(tvNoiseStrength_);
        postProcess->SetClearColorMode(PostClearColorMode::Red);
    }
}

/// <summary>
/// スペースキーまたはマウス左クリック入力を監視し、タイトルシーンへ遷移します。
/// </summary>
void GameOverDirectorScript::OnUpdate() {
    // スペースキーまたは左クリックでタイトルシーンへ遷移
    auto input = InputResource::GetInput();
    if (input && (input->Trigger(DIK_SPACE) || input->MouseTrigger(kLeftMouseButton))) {
        SceneManager::GetInstance()->ChangeScene(nextSceneName_);
        return;
    }
}

void GameOverDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[kImGuiBufferLength];
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
