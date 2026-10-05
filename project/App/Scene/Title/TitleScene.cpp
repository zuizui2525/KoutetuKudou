#include "App/Scene/Title/TitleScene.h"
#include "Engine/Base/BaseResource.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Graphics/Objects/3d/Skybox/Skybox.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <filesystem>
#include <format>

namespace {
    // 2Dテスト用オブジェクト定数 (マジックナンバー排除)
    static inline const Vector3 kTestSpritePosition = { 100.0f, 150.0f, 0.0f };
    static constexpr float kTestSpriteWidth = 120.0f;
    static constexpr float kTestSpriteHeight = 120.0f;

    static inline const Vector3 kTestTrianglePosition = { 280.0f, 150.0f, 0.0f };
    static constexpr float kTestTriangleWidth = 120.0f;
    static constexpr float kTestTriangleHeight = 120.0f;
    static inline const Vector4 kTestTriangleColor = { 0.2f, 0.8f, 1.0f, 1.0f }; // シアン

    static inline const Vector3 kTestCirclePosition = { 460.0f, 150.0f, 0.0f };
    static constexpr float kTestCircleRadius = 60.0f;
    static inline const Vector4 kTestCircleColor = { 1.0f, 0.85f, 0.2f, 1.0f }; // イエロー

    static inline const Vector3 kTestRingPosition = { 640.0f, 150.0f, 0.0f };
    static constexpr float kTestRingOuterRadius = 60.0f;
    static constexpr float kTestRingInnerRadius = 40.0f;
    static inline const Vector4 kTestRingColor = { 1.0f, 0.3f, 0.6f, 1.0f }; // ピンク

    static inline const Vector2 kTestLineStart = { 100.0f, 320.0f };
    static inline const Vector2 kTestLineEnd = { 760.0f, 320.0f };
    static constexpr float kTestLineThickness = 6.0f;
    static inline const Vector4 kTestLineColor = { 0.4f, 1.0f, 0.4f, 1.0f }; // ライムグリーン

    static inline const std::string kDefaultTextureKey = "white";
    static inline const std::string kForestEnvTextureKey = "forestTex";
    static inline const std::string kBunnyModelKey = "bunny";
    static inline const std::string kScenesDirectory = "resources/Scenes";
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
    // 0. ポストプロセスのポインタを取得してメンバ変数に保持し、初期状態で各エフェクトを有効化
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    if (postProcess_) {
        postProcess_->SetDepthOutlineActive(true);
        postProcess_->SetGrayscaleActive(true);
        postProcess_->SetVignetteActive(true);
    }

    // 1. 各マネージャの取得
    cameraMgr_ = CameraResource::GetCameraManager();
    lightMgr_ = LightResource::GetLightManager();
    input_ = InputResource::GetInput();

    // 2. シーンJSONファイルの自動読み込みまたは初期生成
    std::string sceneFilePath = kScenesDirectory + "/Title.json";
    bool fileExisted = std::filesystem::exists(sceneFilePath);
    if (fileExisted) {
        std::string loadedName;
        bool success = SceneSerializer::LoadScene(sceneFilePath, loadedName, gameObjects_);
        if (success) {
            Log::Write(std::format(L"[TitleScene] シーンファイル「{}」から {} 個のオブジェクトを復元しました。",
                ConvertString(sceneFilePath), gameObjects_.size()));
        } else {
            Log::Write(std::format(L"[TitleScene] シーンファイル「{}」の読み込みに失敗しました。",
                ConvertString(sceneFilePath)));
        }
    }

    // カメラの存在確認（なければ生成）
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
        Log::Write(L"[TitleScene] デフォルトの Camera GameObject を追加しました。");
    }

    // ライトの存在確認（なければ生成）
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
        auto* lightComp = lightObj->AddComponent<LightComponent>();
        lightComp->SetLightType(LightComponent::LightType::Directional);
        if (gameObjects_.size() > 1) {
            gameObjects_.insert(gameObjects_.begin() + 1, std::move(lightObj));
        } else {
            gameObjects_.push_back(std::move(lightObj));
        }
        Log::Write(L"[TitleScene] デフォルトの DirectionalLight GameObject を追加しました。");
    }

    // 初回（ファイルが存在しない、またはCamera/Light以外の描画オブジェクトが空の場合）はオブジェクト群を自動生成
    bool hasVisualObjects = false;
    for (const auto& obj : gameObjects_) {
        if (obj && (obj->GetComponent<MeshRendererComponent>() || obj->GetComponent<SpriteRendererComponent>())) {
            hasVisualObjects = true;
            break;
        }
    }

    if (!fileExisted || !hasVisualObjects) {
        auto create3D = [this](const std::string& name, MeshRendererComponent::MeshType type, const Vector3& pos) {
            auto obj = std::make_unique<GameObject>(name);
            obj->SetPosition(pos);
            auto* mr = obj->AddComponent<MeshRendererComponent>();
            mr->SetMeshType(type);
            mr->SetEnvMapKey(kForestEnvTextureKey);
            gameObjects_.push_back(std::move(obj));
        };

        create3D("Cube", MeshRendererComponent::MeshType::Cube, { 2.0f, 2.0f, 0.0f });
        create3D("Sphere", MeshRendererComponent::MeshType::Sphere, { 2.0f, 0.0f, 0.0f });
        create3D("Pyramid", MeshRendererComponent::MeshType::Pyramid, { 4.0f, 2.0f, 0.0f });
        create3D("TriangularPyramid", MeshRendererComponent::MeshType::TriangularPyramid, { 4.0f, -2.0f, 0.0f });
        create3D("Triangle", MeshRendererComponent::MeshType::Triangle, { -2.0f, 2.0f, 0.0f });
        create3D("Square", MeshRendererComponent::MeshType::Square, { -2.0f, 0.0f, 0.0f });
        create3D("Cylinder", MeshRendererComponent::MeshType::Cylinder, { 2.0f, -2.0f, 0.0f });
        create3D("Cone", MeshRendererComponent::MeshType::Cone, { 4.0f, 0.0f, 0.0f });
        create3D("Ring", MeshRendererComponent::MeshType::Ring, { -2.0f, -2.0f, 0.0f });
        create3D("Hemisphere", MeshRendererComponent::MeshType::Hemisphere, { -4.0f, 0.0f, 0.0f });

        // Line 3D
        {
            auto lineObj = std::make_unique<GameObject>("Line");
            lineObj->SetPosition({ 0.0f, 0.0f, 0.0f });
            auto* mr = lineObj->AddComponent<MeshRendererComponent>();
            mr->SetMeshType(MeshRendererComponent::MeshType::Line);
            gameObjects_.push_back(std::move(lineObj));
        }

        // Model "bunny"
        {
            auto bunnyObj = std::make_unique<GameObject>("Bunny");
            bunnyObj->SetPosition({ 0.0f, 0.0f, 0.0f });
            auto* mr = bunnyObj->AddComponent<MeshRendererComponent>();
            mr->SetMeshType(MeshRendererComponent::MeshType::Model);
            mr->SetModelKey(kBunnyModelKey);
            mr->SetEnvMapKey(kForestEnvTextureKey);
            gameObjects_.push_back(std::move(bunnyObj));
        }

        // 2Dオブジェクト群
        {
            auto spriteObj = std::make_unique<GameObject>("Sprite");
            spriteObj->SetPosition(kTestSpritePosition);
            auto* sr = spriteObj->AddComponent<SpriteRendererComponent>();
            sr->SetShapeType(SpriteRendererComponent::ShapeType::Sprite);
            sr->SetSize({ kTestSpriteWidth, kTestSpriteHeight });
            gameObjects_.push_back(std::move(spriteObj));
        }
        {
            auto triObj = std::make_unique<GameObject>("Triangle2D");
            triObj->SetPosition(kTestTrianglePosition);
            auto* sr = triObj->AddComponent<SpriteRendererComponent>();
            sr->SetShapeType(SpriteRendererComponent::ShapeType::Triangle);
            sr->SetSize({ kTestTriangleWidth, kTestTriangleHeight });
            sr->SetColor(kTestTriangleColor);
            gameObjects_.push_back(std::move(triObj));
        }
        {
            auto circleObj = std::make_unique<GameObject>("Circle2D");
            circleObj->SetPosition(kTestCirclePosition);
            auto* sr = circleObj->AddComponent<SpriteRendererComponent>();
            sr->SetShapeType(SpriteRendererComponent::ShapeType::Circle);
            sr->SetRadius(kTestCircleRadius);
            sr->SetColor(kTestCircleColor);
            gameObjects_.push_back(std::move(circleObj));
        }
        {
            auto ringObj = std::make_unique<GameObject>("Ring2D");
            ringObj->SetPosition(kTestRingPosition);
            auto* sr = ringObj->AddComponent<SpriteRendererComponent>();
            sr->SetShapeType(SpriteRendererComponent::ShapeType::Ring);
            sr->SetRadius(kTestRingOuterRadius);
            sr->SetInnerRadius(kTestRingInnerRadius);
            sr->SetColor(kTestRingColor);
            gameObjects_.push_back(std::move(ringObj));
        }
        {
            auto line2dObj = std::make_unique<GameObject>("Line2D");
            line2dObj->SetPosition({ 0.0f, 0.0f, 0.0f });
            auto* sr = line2dObj->AddComponent<SpriteRendererComponent>();
            sr->SetShapeType(SpriteRendererComponent::ShapeType::Line);
            sr->SetLineStart(kTestLineStart);
            sr->SetLineEnd(kTestLineEnd);
            sr->SetLineThickness(kTestLineThickness);
            sr->SetColor(kTestLineColor);
            gameObjects_.push_back(std::move(line2dObj));
        }

        std::filesystem::create_directories(kScenesDirectory);
        SceneSerializer::SaveScene(sceneFilePath, "Title", gameObjects_);
        Log::Write(std::format(L"[TitleScene] 新規シーンファイル「{}」を自動生成しました。", ConvertString(sceneFilePath)));
    }

    // 3. 背景 Skybox の生成
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize();
}

void TitleScene::ImGuiControl() {
#ifdef _USEIMGUI
    if (cameraMgr_) {
        cameraMgr_->ImGuiControl();
    }
    if (postProcess_) {
        postProcess_->ImGuiControl();
    }
#endif
}

void TitleScene::Update() {
    // スペースキーで Game シーンへ遷移
    if (input_ && input_->Trigger(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("Game");
    }

    // 全 GameObject の更新
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }

    // Skybox の更新
    if (skybox_) {
        skybox_->Update();
    }
}

void TitleScene::Draw() {
    // Skybox の描画
    if (skybox_) {
        skybox_->Draw(kForestEnvTextureKey);
    }

    // 全 GameObject の 3D 描画
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw();
        }
    }
}

void TitleScene::Draw2D() {
    // 全 GameObject の 2D 描画
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw2D();
        }
    }
}

GameObject* TitleScene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    gameObjects_.push_back(std::move(obj));
    return ptr;
}

void TitleScene::AddGameObject(std::unique_ptr<GameObject> gameObject) {
    if (gameObject) {
        gameObjects_.push_back(std::move(gameObject));
    }
}

void TitleScene::DestroyGameObject(GameObject* gameObject) {
    std::erase_if(gameObjects_, [gameObject](const std::unique_ptr<GameObject>& obj) {
        return obj.get() == gameObject;
    });
}
