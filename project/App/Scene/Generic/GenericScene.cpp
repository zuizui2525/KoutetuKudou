#include "App/Scene/Generic/GenericScene.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/3d/Skybox/Skybox.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectFactory.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Debug/SceneHierarchy.h"
#include <filesystem>
#include <format>
#include <algorithm>
#include <cstdlib>

namespace {
    const std::string kScenesDirectory = "resources/Scenes";
    const std::string kDefaultMainCameraName = "Main";
    const std::string kDefaultGameObjectPrefix = "GameObject";

    // シーン名定数 (マジックストリング排除)
    const std::string kSceneNameTitle = "Title";
    const std::string kSceneNameGame = "Game";
    const std::string kSceneNameClear = "Clear";
    const std::string kSceneNameGameOver = "GameOver";

    // Titleシーン用定数 (マジックナンバー・ストリング排除)
    const std::string kTitleSkyboxTextureKey = "white";
    constexpr Vector4 kTitleSkyboxColor = { 0.02f, 0.02f, 0.03f, 1.0f }; // 重厚なダークブラック背景

    // Clearシーン用花火定数 (マジックナンバー排除)
    const std::string kFireworkEffectName = "FireworksSet";
    constexpr int kFireworkMinInterval = 30;         // 最小間隔（フレーム）
    constexpr int kFireworkMaxIntervalRange = 61;    // ランダム幅（フレーム）
    constexpr float kFireworkRangeX = 60.0f;         // 打ち上げ位置の左右幅
    constexpr float kFireworkRangeZ = 20.0f;         // 打ち上げ位置の前後幅
    constexpr float kFireworkMinY = -5.0f;           // 打ち上げ位置の高さの最小値

    // GameOverシーン用TVノイズ定数 (マジックナンバー排除)
    constexpr float kGameOverTVNoiseStrength = 0.1f;
}

GenericScene::GenericScene(const std::string& sceneName)
    : sceneName_(sceneName) {
}

GenericScene::~GenericScene() = default;

void GenericScene::Initialize() {
    // ポストプロセスの取得
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    cameraMgr_ = CameraResource::GetCameraManager();

    // シーン固有のポストプロセス演出設定
    if (postProcess_) {
        if (sceneName_ == kSceneNameTitle) {
            postProcess_->SetDepthOutlineActive(true);
            postProcess_->SetGrayscaleActive(false);
            postProcess_->SetVignetteActive(true);
        } else if (sceneName_ == kSceneNameClear) {
            postProcess_->SetRadialBlurActive(true);
        } else if (sceneName_ == kSceneNameGameOver) {
            postProcess_->SetVignetteActive(true);
            postProcess_->SetTVNoiseActive(true);
            postProcess_->SetTVNoiseStrength(kGameOverTVNoiseStrength);
            postProcess_->SetClearColorMode(PostClearColorMode::Red);
        }
    }

    // Titleシーン用 Skybox の初期化
    if (sceneName_ == kSceneNameTitle) {
        skybox_ = std::make_unique<Skybox>();
        skybox_->Initialize();
        skybox_->SetColor(kTitleSkyboxColor);
    }

    // Clearシーン用 エフェクトマネージャの初期化
    if (sceneName_ == kSceneNameClear) {
        auto effectMgr = EffectManager::GetInstance();
        effectMgr->Initialize();
        EffectFactory::GetInstance()->RegisterAllEffects();
        fireworkTimer_ = 0;
    }

    // シーンJSONファイルの自動読み込み
    std::string sceneFilePath = kScenesDirectory + "/" + sceneName_ + ".json";
    bool fileExisted = std::filesystem::exists(sceneFilePath);
    if (fileExisted) {
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

    // ファイルが存在しなかった場合はデフォルト構成で自動保存
    if (!fileExisted) {
        std::filesystem::create_directories(kScenesDirectory);
        SceneSerializer::SaveScene(sceneFilePath, sceneName_, gameObjects_);
        Log::Write(std::format(L"[GenericScene] 新規シーンファイル「{}」を自動生成しました。",
            ConvertString(sceneFilePath)));
    }
}

void GenericScene::Update() {
    // スペースキー押下によるシーン遷移（Title -> Game / Clear or GameOver -> Title）
    auto input = InputResource::GetInput();
    if (input && input->Trigger(DIK_SPACE)) {
        if (sceneName_ == kSceneNameTitle) {
            SceneManager::GetInstance()->ChangeScene(kSceneNameGame);
        } else if (sceneName_ == kSceneNameClear || sceneName_ == kSceneNameGameOver) {
            SceneManager::GetInstance()->ChangeScene(kSceneNameTitle);
        }
    }

    // Skybox の更新
    if (skybox_) {
        skybox_->Update();
    }

    // Clearシーンの花火エフェクト処理
    if (sceneName_ == kSceneNameClear) {
        if (--fireworkTimer_ <= 0) {
            fireworkTimer_ = kFireworkMinInterval + rand() % kFireworkMaxIntervalRange;
            EffectPlayParam param;
            float rx = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * kFireworkRangeX;
            float rz = (static_cast<float>(rand()) / RAND_MAX + 2.0f) * kFireworkRangeZ;
            float ry = (static_cast<float>(rand()) / RAND_MAX + 3.0f) * kFireworkMinY;
            param.position = { rx, ry, rz };
            param.scale = { 1.0f, 1.0f, 1.0f };
            param.isLoop = false;
            EffectManager::GetInstance()->PlayEffect3D(kFireworkEffectName, param);
        }
        EffectManager::GetInstance()->Update();
    }

    // 全 GameObject の更新
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void GenericScene::Draw() {
    // Skybox の描画 (Titleシーン等)
    if (skybox_) {
        skybox_->Draw(kTitleSkyboxTextureKey);
    }

    // 全 GameObject の 3D 描画
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw();
        }
    }

    // Clearシーンのエフェクト描画
    if (sceneName_ == kSceneNameClear) {
        EffectManager::GetInstance()->Draw();
    }
}

void GenericScene::Draw2D() {
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw2D();
        }
    }
}

void GenericScene::ImGuiControl() {
#ifdef _USEIMGUI
    if (cameraMgr_) {
        cameraMgr_->ImGuiControl();
    }
    if (postProcess_) {
        postProcess_->ImGuiControl();
    }
#endif
}

GameObject* GenericScene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* rawPtr = obj.get();
    gameObjects_.push_back(std::move(obj));
    return rawPtr;
}

void GenericScene::AddGameObject(std::unique_ptr<GameObject> gameObject) {
    if (gameObject) {
        SceneHierarchy::GetInstance()->Register(gameObject.get(), gameObject->GetName());
        gameObjects_.push_back(std::move(gameObject));
    }
}

void GenericScene::DestroyGameObject(GameObject* gameObject) {
    if (!gameObject) {
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

std::unique_ptr<GameObject> GenericScene::DetachGameObject(GameObject* gameObject) {
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
