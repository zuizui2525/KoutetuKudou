#pragma once
#include "App/Scene/Core/IScene.h"
#include "App/Scene/Core/SceneDirector.h"
#include <memory>
#include <string>
#include <vector>

class PostProcess;
class CameraManager;
class GameObject;

/// <summary>
/// Unity/Unreal Engine スタイルの標準シーンクラス
/// ワールド空間のオブジェクト群を管理し、専属の SceneDirector を統括する
/// </summary>
class Scene : public IScene {
public:
    explicit Scene(const std::string& sceneName);
    ~Scene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Draw2D() override;
    void ImGuiControl() override;

    // GameObject 管理
    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void AddGameObject(std::unique_ptr<GameObject> gameObject) override;
    void DestroyGameObject(GameObject* gameObject) override;
    std::unique_ptr<GameObject> DetachGameObject(GameObject* gameObject) override;
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }

    const std::string& GetSceneName() const { return sceneName_; }

    // シーン専属ディレクター（神クラス化防止のためのゲームロジック統括役）
    SceneDirector* GetDirector() const { return director_.get(); }
    void SetDirector(std::unique_ptr<SceneDirector> director) { director_ = std::move(director); }

private:
    std::string sceneName_;

    // 基本システムリソース
    CameraManager* cameraMgr_ = nullptr;
    PostProcess* postProcess_ = nullptr;

    // シーン専属ディレクター
    std::unique_ptr<SceneDirector> director_;

    // シーン所属の GameObject 配列
    std::vector<std::unique_ptr<GameObject>> gameObjects_;

    // 動的追加・破棄の遅延キュー
    bool isUpdating_ = false;
    std::vector<std::unique_ptr<GameObject>> pendingAddObjects_;
    std::vector<GameObject*> pendingDestroyObjects_;
};

// 互換性のためのエイリアス
using GenericScene = Scene;
