#ifdef _USEIMGUI
#include "Engine/Debug/SceneManagerWindow.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneMemoryCache.h"
#include "externals/imgui/imgui.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace {
    const std::filesystem::path kScenesDirectory = "resources/Scenes";
    const std::string kJsonExtension = ".json";

    // 利用可能なシーン名一覧を取得（フォルダスキャン + フォールバック）
    std::vector<std::string> GetAvailableSceneNames() {
        std::vector<std::string> scenes;
        std::error_code ec;
        if (std::filesystem::exists(kScenesDirectory, ec) && std::filesystem::is_directory(kScenesDirectory, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(kScenesDirectory, ec)) {
                if (entry.is_regular_file() && entry.path().extension() == kJsonExtension) {
                    scenes.push_back(entry.path().stem().string());
                }
            }
        }
        if (scenes.empty()) {
            scenes = { "Clear", "Debug", "Game", "GameOver", "Sample", "Title" };
        }
        std::sort(scenes.begin(), scenes.end());
        return scenes;
    }
}

void SceneManagerWindow::Draw(bool* show) {
    if (ImGui::Begin("シーン管理###Scene Manager", show)) {
        ImGui::Text("現在のシーン: %s", SceneManager::GetInstance()->GetCurrentSceneName().c_str());

        ImGui::Separator();
        ImGui::Text("【起動シーン設定】");

        // 起動シーン設定（デバッグ / 本番）
        std::string currentDebugScene = SceneManager::GetInstance()->GetDebugStartupSceneName();
        std::string currentReleaseScene = SceneManager::GetInstance()->GetReleaseStartupSceneName();
        std::vector<std::string> sceneList = GetAvailableSceneNames();

        int debugItemIndex = -1;
        int releaseItemIndex = -1;
        std::vector<const char*> itemNames;
        itemNames.reserve(sceneList.size());
        for (size_t i = 0; i < sceneList.size(); ++i) {
            itemNames.push_back(sceneList[i].c_str());
            if (sceneList[i] == currentDebugScene) {
                debugItemIndex = static_cast<int>(i);
            }
            if (sceneList[i] == currentReleaseScene) {
                releaseItemIndex = static_cast<int>(i);
            }
        }

        // デバッグ起動シーン選択
        if (ImGui::Combo("デバッグ起動シーン", &debugItemIndex, itemNames.data(), static_cast<int>(itemNames.size()))) {
            if (debugItemIndex >= 0 && debugItemIndex < static_cast<int>(sceneList.size())) {
                SceneManager::GetInstance()->SetDebugStartupSceneName(sceneList[debugItemIndex]);
            }
        }

        // 本番起動シーン選択
        if (ImGui::Combo("本番(Release)起動シーン", &releaseItemIndex, itemNames.data(), static_cast<int>(itemNames.size()))) {
            if (releaseItemIndex >= 0 && releaseItemIndex < static_cast<int>(sceneList.size())) {
                SceneManager::GetInstance()->SetReleaseStartupSceneName(sceneList[releaseItemIndex]);
            }
        }

        // 現在のシーンを設定するボタン群
        const std::string currentScene = SceneManager::GetInstance()->GetCurrentSceneName();
        if (!currentScene.empty() && currentScene != "None") {
            if (ImGui::Button("現在のシーンをデバッグ起動に設定")) {
                SceneManager::GetInstance()->SetDebugStartupSceneName(currentScene);
            }
            ImGui::SameLine();
            if (ImGui::Button("現在のシーンを本番起動に設定")) {
                SceneManager::GetInstance()->SetReleaseStartupSceneName(currentScene);
            }
        }

        ImGui::Separator();
        ImGui::Text("【シーン即時遷移・キャッシュリセット】");

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
