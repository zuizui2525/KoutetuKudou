#include "App/Scene/Core/SceneFactory.h"
#include "App/Scene/Debug/DebugScene.h"
#include "App/Scene/Game/GameScene.h"
#include "App/Scene/Generic/GenericScene.h"

std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName) {
    std::unique_ptr<IScene> newScene = nullptr;

    // 文字列の直接使用（マジックストリング）を回避するための定数定義
    static const std::string kDebugSceneName = "Debug";
    static const std::string kGameSceneName = "Game";

    if (sceneName == kDebugSceneName) {
        newScene = std::make_unique<DebugScene>();
    } else if (sceneName == kGameSceneName) {
        newScene = std::make_unique<GameScene>();
    } else {
        // コンポーネント指向シーン（Title, Clear, GameOver, Sample, 独自シーン等）はすべて汎用シーンとして生成
        newScene = std::make_unique<GenericScene>(sceneName);
    }

    return newScene;
}
