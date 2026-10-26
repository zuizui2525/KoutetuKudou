#pragma once
#include <memory>
#include <string>
#include "App/Scene/Core/IScene.h"
#include "App/Scene/Core/AbstractSceneFactory.h"
#include "Engine/Graphics/Objects/3d/Frustum/CameraFrustumObject.h"
#include "Engine/Graphics/Objects/3d/LightGizmo/LightGizmoObject.h"

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

    void Initialize();
    void Update();
    void Draw();
    void Draw2D();
    void ImGuiControl();

    /// @brief 予約されたシーン遷移（nextSceneName_）が存在する場合に即座に実行する（ポーズ中でも実行可能）
    /// @return シーン遷移が実行された場合は true
    bool ProcessPendingSceneChange();

    void SetPostProcess(PostProcess* postProcess);
    PostProcess* GetPostProcess() const { return postProcess_; }

    /**
     * @brief 次のシーンへの遷移を要求する（トランジション演出対応）
     * @param sceneName 遷移先シーン名
     * @param useTransition 画面切り替えトランジションを適用するかどうか
     */
    void ChangeScene(const std::string& sceneName, bool useTransition = true);

    /**
     * @brief トランジションなしで即座にシーンを切り替える
     */
    void ChangeSceneImmediate(const std::string& sceneName);

    /// @brief デバッグビルド時の起動シーン名を取得（設定ファイルから読み込み、未設定時はデフォルト）
    std::string GetDebugStartupSceneName();

    /// @brief デバッグビルド時の起動シーン名を設定ファイルに保存
    void SetDebugStartupSceneName(const std::string& sceneName);

    /// @brief リリースビルド時の本番開始シーン名を取得（設定ファイルから読み込み、未設定時はデフォルト）
    std::string GetReleaseStartupSceneName();

    /// @brief リリースビルド時の本番開始シーン名を設定ファイルに保存
    void SetReleaseStartupSceneName(const std::string& sceneName);

    /**
     * @brief 使用する工場（Factory）を設定する
     * アプリ起動時に一回呼ぶ必要がある
     */
    void SetSceneFactory(AbstractSceneFactory* factory) {
        sceneFactory_ = factory;
    }

    void ClearCurrentScene();
    const std::string& GetCurrentSceneName() const { return currentSceneName_; }
    IScene* GetCurrentScene() const { return currentScene_.get(); }

    /**
     * @brief プレハブから新しいGameObjectを動的に実体化し、現在のシーンに追加する
     * @param prefabName プレハブ名（例: "Bullet", "Enemy"）
     * @param position 初期配置座標
     * @param rotation 初期回転角
     * @return 生成されたGameObjectの生ポインタ（失敗時はnullptr）
     */
    GameObject* Instantiate(
        const std::string& prefabName,
        const Vector3& position = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f }
    );

    /**
     * @brief 指定されたGameObjectを安全に現在のシーンから破棄予約する
     * @param obj 破棄対象のGameObject
     */
    void Destroy(GameObject* obj);

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
    std::unique_ptr<CameraFrustumObject> dbgCameraModel_ = nullptr;
    std::unique_ptr<LightGizmoObject> dbgLightModel_ = nullptr;
};
