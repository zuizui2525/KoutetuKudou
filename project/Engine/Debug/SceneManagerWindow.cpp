#ifdef _USEIMGUI
#include "Engine/Debug/SceneManagerWindow.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneMemoryCache.h"
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
            SceneMemoryCache::GetInstance()->ClearCache(kDebugSceneName);
            SceneManager::GetInstance()->ChangeScene(kDebugSceneName, false);
        }
        if (ImGui::Button("タイトルシーンにリセット")) {
            SceneMemoryCache::GetInstance()->ClearCache(kTitleSceneName);
            SceneManager::GetInstance()->ChangeScene(kTitleSceneName, false);
        }
        if (ImGui::Button("ゲームシーンにリセット")) {
            SceneMemoryCache::GetInstance()->ClearCache(kGameSceneName);
            SceneManager::GetInstance()->ChangeScene(kGameSceneName, false);
        }
        if (ImGui::Button("クリアシーンにリセット")) {
            SceneMemoryCache::GetInstance()->ClearCache(kClearSceneName);
            SceneManager::GetInstance()->ChangeScene(kClearSceneName, false);
        }
        if (ImGui::Button("ゲームオーバーシーンにリセット")) {
            SceneMemoryCache::GetInstance()->ClearCache(kGameOverSceneName);
            SceneManager::GetInstance()->ChangeScene(kGameOverSceneName, false);
        }
    }
    ImGui::End();
}
#endif
