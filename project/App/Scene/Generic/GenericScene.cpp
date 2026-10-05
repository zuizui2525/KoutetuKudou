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
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Debug/SceneHierarchy.h"
#include <filesystem>
#include <format>
#include <algorithm>

namespace {
    const std::string kScenesDirectory = "resources/Scenes";
    const std::string kDefaultMainCameraName = "Main";
    const std::string kDefaultGameObjectPrefix = "GameObject";
}

GenericScene::GenericScene(const std::string& sceneName)
    : sceneName_(sceneName) {
}

GenericScene::~GenericScene() = default;

void GenericScene::Initialize() {
    // ポストプロセスの取得
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    cameraMgr_ = CameraResource::GetCameraManager();

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
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void GenericScene::Draw() {
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw();
        }
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
