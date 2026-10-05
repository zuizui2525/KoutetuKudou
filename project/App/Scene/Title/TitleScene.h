#pragma once
#include "App/Scene/Core/IScene.h"
#include <memory>
#include <vector>
#include <string>

class PostProcess;
class Input;
class CameraManager;
class LightManager;
class GameObject;
class Skybox;

class TitleScene : public IScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void ImGuiControl() override;
    void Update() override;
    void Draw() override;
    void Draw2D() override;

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void AddGameObject(std::unique_ptr<GameObject> gameObject);
    void DestroyGameObject(GameObject* gameObject);
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }

private:
    // マネージャへのポインタ
    Input* input_ = nullptr;
    CameraManager* cameraMgr_ = nullptr;
    LightManager* lightMgr_ = nullptr;
    PostProcess* postProcess_ = nullptr;

    // コンポーネント指向 GameObject コレクション
    std::vector<std::unique_ptr<GameObject>> gameObjects_;

    // 背景 Skybox
    std::unique_ptr<Skybox> skybox_;
};
