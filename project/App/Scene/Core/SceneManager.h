#pragma once
#include <memory>
#include <string>
#include "App/Scene/Core/IScene.h"
#include "App/Scene/Core/AbstractSceneFactory.h"
#include "Engine/Graphics/Objects/3d/Pyramid/PyramidObject.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"

class PostProcess;


/**
 * @brief シーン管理クラス（シングルトン）
 * * 指摘ポイント：
 * 1. テンプレートによる生成を廃止し、Factory経由に変更。
 * 2. 具体的なシーンクラスをインクルードしない（依存性逆転）。
 */
class SceneManager {
public:
    static SceneManager* GetInstance();

    void Update();
    void Draw();
    void Draw2D();
    void ImGuiControl();

    void SetPostProcess(PostProcess* postProcess) { postProcess_ = postProcess; }
    PostProcess* GetPostProcess() const { return postProcess_; }


    /**
     * @brief 次のシーンを名前で予約する
     * 旧：template <typename T> void ChangeScene(...)
     * 新：文字列のみを受け取る
     */
    void ChangeScene(const std::string& sceneName) {
        nextSceneName_ = sceneName;
    }

    /**
     * @brief 使用する工場（Factory）を設定する
     * アプリ起動時に一回呼ぶ必要がある
     */
    void SetSceneFactory(AbstractSceneFactory* factory) {
        sceneFactory_ = factory;
    }

    void ClearCurrentScene() { 
        currentScene_.reset(); 
        dbgCameraModel_.reset();
        dbgLightModel_.reset();
    }
    const std::string& GetCurrentSceneName() const { return currentSceneName_; }
    IScene* GetCurrentScene() const { return currentScene_.get(); }

private:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // 現在使用中の工場へのポインタ
    AbstractSceneFactory* sceneFactory_ = nullptr;

    std::unique_ptr<IScene> currentScene_ = nullptr;
    std::unique_ptr<IScene> nextScene_ = nullptr;
    std::string currentSceneName_ = "None";
    std::string nextSceneName_ = "";

    PostProcess* postProcess_ = nullptr;

    // デバッグ用表示モデル（リーク対策としてメンバ変数で管理）
    std::unique_ptr<PyramidObject> dbgCameraModel_ = nullptr;
    std::unique_ptr<SphereObject> dbgLightModel_ = nullptr;
};
