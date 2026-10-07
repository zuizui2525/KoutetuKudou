#pragma once
#include "App/Scene/Core/IScene.h"
#include <memory>
#include <string>
#include <vector>

class PostProcess;
class BaseCamera;
class CameraManager;
class DirectionalLightObject;
class GameObject;
class Skybox;

/// <summary>
/// Unity/Unreal Engine スタイルの汎用コンポーネント指向シーン
/// resources/Scenes/<sceneName>.json から GameObject 群を自動復元・管理する
/// </summary>
class GenericScene : public IScene {
public:
    explicit GenericScene(const std::string& sceneName);
    ~GenericScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Draw2D() override;
    void ImGuiControl() override;

    // GameObject 管理
    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void AddGameObject(std::unique_ptr<GameObject> gameObject) override;
    void DestroyGameObject(GameObject* gameObject);
    std::unique_ptr<GameObject> DetachGameObject(GameObject* gameObject) override;
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }

    const std::string& GetSceneName() const { return sceneName_; }

private:
    std::string sceneName_;

    // 基本システムリソース
    CameraManager* cameraMgr_ = nullptr;
    std::shared_ptr<BaseCamera> camera_;
    std::unique_ptr<DirectionalLightObject> dirLight_;
    PostProcess* postProcess_ = nullptr;

    // シーン所属の GameObject 配列
    std::vector<std::unique_ptr<GameObject>> gameObjects_;

    // 背景 Skybox（Titleシーン等で使用）
    std::unique_ptr<Skybox> skybox_;

    // 花火演出タイマー（Clearシーン等で使用）
    int fireworkTimer_ = 0;
};
