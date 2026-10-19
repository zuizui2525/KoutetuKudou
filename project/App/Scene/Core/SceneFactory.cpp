#include "App/Scene/Core/SceneFactory.h"
#include "App/Scene/Generic/GenericScene.h"

std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName) {
    std::unique_ptr<IScene> newScene = nullptr;

    // すべてのシーン（Title, Game, Clear, GameOver, Sample, Debug等）を汎用コンポーネント指向シーンとして生成
    newScene = std::make_unique<GenericScene>(sceneName);
    return newScene;
}
