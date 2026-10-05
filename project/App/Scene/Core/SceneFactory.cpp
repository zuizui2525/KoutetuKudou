#include "App/Scene/Core/SceneFactory.h"
#include "App/Scene/Title/TitleScene.h"
#include "App/Scene/Debug/DebugScene.h"
#include "App/Scene/Game/GameScene.h"
#include "App/Scene/Clear/ClearScene.h"
#include "App/Scene/GameOver/GameOverScene.h"
#include "App/Scene/Generic/GenericScene.h"

std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName) {
    std::unique_ptr<IScene> newScene = nullptr;

    // 文字列の直接使用（マジックナンバー・マジックストリング）を回避するための定数定義
    static const std::string kTitleSceneName = "Title";
    static const std::string kDebugSceneName = "Debug";
    static const std::string kGameSceneName = "Game";
    static const std::string kClearSceneName = "Clear";
    static const std::string kGameOverSceneName = "GameOver";

    if (sceneName == kTitleSceneName) {
        newScene = std::make_unique<TitleScene>();
    } else if (sceneName == kDebugSceneName) {
        newScene = std::make_unique<DebugScene>();
    } else if (sceneName == kGameSceneName) {
        newScene = std::make_unique<GameScene>();
    } else if (sceneName == kClearSceneName) {
        newScene = std::make_unique<ClearScene>();
    } else if (sceneName == kGameOverSceneName) {
        newScene = std::make_unique<GameOverScene>();
    } else {
        // 固定C++クラスのないシーン（Sample, NewScene等）は汎用コンポーネントシーンとして生成
        newScene = std::make_unique<GenericScene>(sceneName);
    }

    return newScene;
}
