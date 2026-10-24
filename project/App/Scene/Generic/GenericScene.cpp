#include "App/Scene/Core/Scene.h"
#include "App/Scene/Core/SceneDirector.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "App/Scene/Core/SceneMemoryCache.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include <filesystem>
#include <format>
#include <algorithm>

namespace {
    const std::string kScenesDirectory = "resources/Scenes";
}

Scene::Scene(const std::string& sceneName)
    : sceneName_(sceneName) {
}

Scene::~Scene() = default;

void Scene::Initialize() {
    // ポストプロセスの取得
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    cameraMgr_ = CameraResource::GetCameraManager();

    std::string sceneFilePath = kScenesDirectory + "/" + sceneName_ + ".json";
    bool fileExisted = std::filesystem::exists(sceneFilePath);

    // シーンデータの自動読み込み（メモリ一時キャッシュを最優先、無ければディスクの .json から読み込む）
    bool loaded = false;
    if (SceneMemoryCache::GetInstance()->HasCache(sceneName_)) {
        std::string cachedJson = SceneMemoryCache::GetInstance()->GetCache(sceneName_);
        std::string loadedName;
        loaded = SceneSerializer::DeserializeFromString(cachedJson, loadedName, gameObjects_);
        if (loaded) {
            Log::Write(std::format(L"[GenericScene] メモリ一時キャッシュからシーン「{}」({} 個のオブジェクト) を復元しました。",
                ConvertString(sceneName_), gameObjects_.size()));
        }
    }

    if (!loaded && fileExisted) {
        std::string loadedName;
        bool success = SceneSerializer::LoadScene(sceneFilePath, loadedName, gameObjects_);
        if (success) {
            Log::Write(std::format(L"[GenericScene] シーンファイル「{}」から {} 個のオブジェクトを復元しました。",
                ConvertString(sceneFilePath), gameObjects_.size()));
        } else {
            Log::Write(std::format(L"[GenericScene] シーンファイル「{}」の読み込みに失敗しました。",
                ConvertString(sceneFilePath)));
        }
    }

    // シーン内に CameraComponent が存在するかチェック（無ければデフォルトカメラを生成）
    bool hasCamera = false;
    for (const auto& obj : gameObjects_) {
        if (obj && obj->GetComponent<CameraComponent>()) {
            hasCamera = true;
            break;
        }
    }
    if (!hasCamera) {
        auto camObj = std::make_unique<GameObject>("Camera");
        camObj->SetPosition({ 0.0f, 0.0f, -20.0f });
        camObj->AddComponent<CameraComponent>();
        gameObjects_.insert(gameObjects_.begin(), std::move(camObj));
        Log::Write(L"[GenericScene] デフォルトの Camera GameObject を追加しました。");
    }

    // シーン内に LightComponent が存在するかチェック（無ければデフォルトライトを生成）
    bool hasLight = false;
    for (const auto& obj : gameObjects_) {
        if (obj && obj->GetComponent<LightComponent>()) {
            hasLight = true;
            break;
        }
    }
    if (!hasLight) {
        auto lightObj = std::make_unique<GameObject>("DirectionalLight");
        lightObj->SetPosition({ 0.0f, 10.0f, 0.0f });
        auto* light = lightObj->AddComponent<LightComponent>();
        light->SetLightType(LightComponent::LightType::Directional);
        if (gameObjects_.size() > 1) {
            gameObjects_.insert(gameObjects_.begin() + 1, std::move(lightObj));
        } else {
            gameObjects_.push_back(std::move(lightObj));
        }
        Log::Write(L"[GenericScene] デフォルトの DirectionalLight GameObject を追加しました。");
    }

    // シーン専属ディレクター（SceneDirector）の自動生成と各シーン標準スクリプトの設定
    if (!director_) {
        director_ = std::make_unique<SceneDirector>(this);
    }

    if (director_->GetScripts().empty()) {
        if (sceneName_ == "Game") {
            auto routeMgr = ScriptRegistry::GetInstance().CreateScript("RouteManagerScript");
            auto enemySpawn = ScriptRegistry::GetInstance().CreateScript("EnemySpawnManagerScript");
            auto phaseCtrl = ScriptRegistry::GetInstance().CreateScript("GamePhaseControllerScript");
            auto routeRenderer = ScriptRegistry::GetInstance().CreateScript("RouteRendererScript");
            auto stageMgr = ScriptRegistry::GetInstance().CreateScript("StageManagerScript");

            if (routeMgr) director_->AddScript(std::move(routeMgr));
            if (enemySpawn) director_->AddScript(std::move(enemySpawn));
            if (phaseCtrl) director_->AddScript(std::move(phaseCtrl));
            if (routeRenderer) director_->AddScript(std::move(routeRenderer));
            if (stageMgr) director_->AddScript(std::move(stageMgr));
        } else if (sceneName_ == "Title") {
            auto titleDir = ScriptRegistry::GetInstance().CreateScript("TitleDirectorScript");
            if (titleDir) director_->AddScript(std::move(titleDir));
        } else if (sceneName_ == "Clear") {
            auto clearDir = ScriptRegistry::GetInstance().CreateScript("ClearDirectorScript");
            if (clearDir) director_->AddScript(std::move(clearDir));
        } else if (sceneName_ == "GameOver") {
            auto gameOverDir = ScriptRegistry::GetInstance().CreateScript("GameOverDirectorScript");
            if (gameOverDir) director_->AddScript(std::move(gameOverDir));
        }
    }
    director_->Initialize();
    Log::Write(std::format(L"[Scene] シーン「{}」の SceneDirector を初期化しました。", ConvertString(sceneName_)));

    // 旧バージョンの空のDirectorゲームオブジェクト（GameDirector等）をシーンから完全に除去
    auto legacyIt = std::remove_if(gameObjects_.begin(), gameObjects_.end(),
        [](const std::unique_ptr<GameObject>& obj) {
            if (!obj) return false;
            const std::string& n = obj->GetName();
            return (n == "GameDirector" || n == "TitleDirector" || n == "ClearDirector" || n == "GameOverDirector");
        });
    if (legacyIt != gameObjects_.end()) {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
        for (auto iter = legacyIt; iter != gameObjects_.end(); ++iter) {
            if (*iter) {
                SceneHierarchy::GetInstance()->Unregister(iter->get());
            }
        }
        gameObjects_.erase(legacyIt, gameObjects_.end());
    }

    // ファイルが存在しなかった場合はメモリ上のデフォルト構成のまま開始（勝手にディスク保存して復活させない）
    if (!fileExisted) {
        Log::Write(std::format(L"[Scene] シーンファイル「{}」が存在しないため、デフォルト構成で開始します。",
            ConvertString(sceneFilePath)));
    }
}

void Scene::Update() {
    // 1. シーン専属ディレクターの更新（フェーズ制御・進行管理を最優先実行）
    if (director_) {
        director_->Update();
    }

    // 2. 全 GameObject の更新（イテレーション中の動的生成・破棄に耐えうるようインデックスで安全にループ）
    isUpdating_ = true;
    for (size_t i = 0; i < gameObjects_.size(); ++i) {
        if (gameObjects_[i]) {
            gameObjects_[i]->Update();
        }
    }
    isUpdating_ = false;

    // 遅延追加キューの処理（Instantiate等で生成されたオブジェクトをシーンにマージ）
    if (!pendingAddObjects_.empty()) {
        for (auto& newObj : pendingAddObjects_) {
            if (newObj) {
                SceneHierarchy::GetInstance()->Register(newObj.get(), newObj->GetName());
                gameObjects_.push_back(std::move(newObj));
            }
        }
        pendingAddObjects_.clear();
    }

    // 遅延削除キューの処理（DestroyGameObject で破棄予約されたオブジェクトを一括破棄）
    if (!pendingDestroyObjects_.empty()) {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
        for (auto* deadObj : pendingDestroyObjects_) {
            if (!deadObj) continue;
            SceneHierarchy::GetInstance()->Unregister(deadObj);
            auto it = std::remove_if(gameObjects_.begin(), gameObjects_.end(),
                [deadObj](const std::unique_ptr<GameObject>& ptr) {
                    return ptr.get() == deadObj;
                });
            if (it != gameObjects_.end()) {
                gameObjects_.erase(it, gameObjects_.end());
            }
        }
        pendingDestroyObjects_.clear();
    }
}

void Scene::Draw() {
    // 1. 全 GameObject の 3D 描画
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw();
        }
    }

    // 2. シーン専属ディレクターの 3D 描画（3Dルート線等）
    if (director_) {
        director_->Draw();
    }

    // 3. 3Dエフェクト・パーティクルの描画
    if (auto* effectMgr = EffectManager::GetInstance()) {
        effectMgr->Draw();
    }
}

void Scene::Draw2D() {
    // 1. 各 GameObject の 2D 描画
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw2D();
        }
    }

    // 2. シーン専属ディレクターの 2D 描画（UI・マップ等の最前面描画）
    if (director_) {
        director_->Draw2D();
    }
}

void Scene::ImGuiControl() {
#ifdef _USEIMGUI
    if (cameraMgr_) {
        cameraMgr_->ImGuiControl();
    }
    if (postProcess_) {
        postProcess_->ImGuiControl();
    }

    // シーン内 GameObject のスクリプトコンポーネントによるゲーム画面 HUD / UI 描画 (UnityのOnGUI相当)
    for (auto& obj : gameObjects_) {
        if (obj && obj->IsVisible()) {
            for (const auto& comp : obj->GetComponents()) {
                if (comp && comp->IsActive()) {
                    if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
                        script->OnGUI();
                    }
                }
            }
        }
    }

    // シーン専属ディレクター（[Scene] Game等）のスクリプトによる OnGUI 描画
    if (director_) {
        for (const auto& script : director_->GetScripts()) {
            if (script && script->IsActive()) {
                script->OnGUI();
            }
        }
    }
#endif
}

GameObject* Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* rawPtr = obj.get();
    gameObjects_.push_back(std::move(obj));
    return rawPtr;
}

void Scene::AddGameObject(std::unique_ptr<GameObject> gameObject) {
    if (!gameObject) return;

    if (isUpdating_) {
        // Update実行中はイテレータ破壊を防ぐため待機キューに退避
        pendingAddObjects_.push_back(std::move(gameObject));
    } else {
        SceneHierarchy::GetInstance()->Register(gameObject.get(), gameObject->GetName());
        gameObjects_.push_back(std::move(gameObject));
    }
}

void Scene::DestroyGameObject(GameObject* gameObject) {
    if (!gameObject) {
        return;
    }

    if (isUpdating_) {
        // Update実行中はイテレータ破壊を防ぐため即座に非表示にし待機キューに退避
        gameObject->SetVisible(false);
        if (std::find(pendingDestroyObjects_.begin(), pendingDestroyObjects_.end(), gameObject) == pendingDestroyObjects_.end()) {
            pendingDestroyObjects_.push_back(gameObject);
        }
        return;
    }

    // 描画コマンド実行中にバッファが解放されてクラッシュするのを防ぐため GPU を同期
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    // デバッグヒエラルキーからの選択解除・登録解除
    SceneHierarchy::GetInstance()->Unregister(gameObject);

    // シーンのリストから削除（unique_ptr が解放される）
    auto it = std::remove_if(gameObjects_.begin(), gameObjects_.end(),
        [gameObject](const std::unique_ptr<GameObject>& ptr) {
            return ptr.get() == gameObject;
        });

    if (it != gameObjects_.end()) {
        gameObjects_.erase(it, gameObjects_.end());
    }
}

std::unique_ptr<GameObject> Scene::DetachGameObject(GameObject* gameObject) {
    if (!gameObject) return nullptr;

    // GPU同期
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    // ヒエラルキーから解除
    SceneHierarchy::GetInstance()->Unregister(gameObject);

    // リストから所有権を取り出して返す
    auto it = std::find_if(gameObjects_.begin(), gameObjects_.end(),
        [gameObject](const std::unique_ptr<GameObject>& ptr) {
            return ptr.get() == gameObject;
        });

    if (it != gameObjects_.end()) {
        std::unique_ptr<GameObject> detached = std::move(*it);
        gameObjects_.erase(it);
        return detached;
    }
    return nullptr;
}
