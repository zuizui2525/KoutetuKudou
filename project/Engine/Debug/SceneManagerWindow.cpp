#ifdef _USEIMGUI
#include "Engine/Debug/SceneManagerWindow.h"
#include "App/Scene/Core/SceneManager.h"
#include "externals/imgui/imgui.h"
#include <string>

void SceneManagerWindow::Draw(bool* show) {
    // 全シーン共通のデバッグメニュー
    if (ImGui::Begin("シーン管理###Scene Manager", show)) {
        ImGui::Text("現在のシーン: %s", SceneManager::GetInstance()->GetCurrentSceneName().c_str());

        // マジックストリング回避のためのローカル定数定義
        static const std::string kDebugSceneName = "Debug";
        static const std::string kTitleSceneName = "Title";
        static const std::string kGameSceneName = "Game";
        static const std::string kClearSceneName = "Clear";
        static const std::string kGameOverSceneName = "GameOver";

        if (ImGui::Button("デバッグシーンにリセット")) {
            SceneManager::GetInstance()->ChangeScene(kDebugSceneName);
        }
        if (ImGui::Button("タイトルシーンにリセット")) {
            SceneManager::GetInstance()->ChangeScene(kTitleSceneName);
        }
        if (ImGui::Button("ゲームシーンにリセット")) {
            SceneManager::GetInstance()->ChangeScene(kGameSceneName);
        }
        if (ImGui::Button("クリアシーンにリセット")) {
            SceneManager::GetInstance()->ChangeScene(kClearSceneName);
        }
        if (ImGui::Button("ゲームオーバーシーンにリセット")) {
            SceneManager::GetInstance()->ChangeScene(kGameOverSceneName);
        }
    }
    ImGui::End();
}
#endif
