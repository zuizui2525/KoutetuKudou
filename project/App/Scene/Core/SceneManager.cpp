#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneTransitionManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Graphics/Objects/3d/Frustum/CameraFrustumObject.h"
#include "Engine/Graphics/Objects/3d/LightGizmo/LightGizmoObject.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/PrefabManager.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/Command/CommandHistory.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include <format>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

void SceneManager::Initialize() {
    auto transitionMgr = SceneTransitionManager::GetInstance();
    transitionMgr->Initialize();
    transitionMgr->SetOnSceneSwitchCallback([this](const std::string& sceneName) {
        // 即座に破棄せず、Update冒頭（フレーム境界）で安全に切り替えるよう予約のみ行う
        this->nextSceneName_ = sceneName;
    });
}

void SceneManager::SetPostProcess(PostProcess* postProcess) {
    postProcess_ = postProcess;
    SceneTransitionManager::GetInstance()->SetPostProcess(postProcess);
}

void SceneManager::ChangeScene(const std::string& sceneName, bool useTransition) {
    if (useTransition) {
        SceneTransitionManager::GetInstance()->StartTransition(currentSceneName_, sceneName);
    } else {
        ChangeSceneImmediate(sceneName);
    }
}

void SceneManager::ChangeSceneImmediate(const std::string& sceneName) {
    nextSceneName_ = sceneName;
}

void SceneManager::ClearCurrentScene() {
    currentScene_.reset();
    dbgCameraModel_.reset();
    dbgLightModel_.reset();
    CommandHistory::GetInstance()->Clear();
}

void SceneManager::ImGuiControl() {
    if (currentScene_) {
        currentScene_->ImGuiControl();
    }
}

bool SceneManager::ProcessPendingSceneChange() {
    // 次のシーン名が入っていなければ処理しない
    if (nextSceneName_.empty()) return false;

    // 工場がセットされていない場合はエラー
    if (!sceneFactory_) return false;

    // シーン切り替え直前に GPU の完了を同期待機（古いリソース描画中の破棄クラッシュ OBJECT_DELETED を防止）
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    // 開始前に空行を挿入して可読性を向上
    Log::Write(L"");

    Log::Write(L"========================================= [シーン切り替え開始] =========================================");
    Log::Write(std::format(L" ├─ 【シーン遷移開始】 新しいシーン「{}」への遷移を開始します。古いシーン「{}」を破棄します。", ConvertString(nextSceneName_), ConvertString(currentSceneName_)));

    // 【Factory Methodパターンの核心】
    // 名前（文字列）を工場に渡し、具体的なクラスを意識せずにインスタンスを得る
    nextScene_ = sceneFactory_->CreateScene(nextSceneName_);

    if (nextScene_) {
        // ライトとカメラおよびシーン階層（選択状態）のリセット
        CameraResource::GetCameraManager()->Clear();
        LightResource::GetLightManager()->Clear();
        SceneHierarchy::GetInstance()->Clear();
        CommandHistory::GetInstance()->Clear();

        // ポストプロセスのエフェクトおよびクリアカラーのリセット（シーン遷移時の自動解除）
        if (postProcess_) {
            postProcess_->ClearEffects();
            postProcess_->SetClearColorMode(PostClearColorMode::Blue);
        }

        // 古いシーンを破棄して新しいシーンへ
        currentScene_ = std::move(nextScene_);
        currentSceneName_ = nextSceneName_;

        Log::Write(std::format(L" ├─ 【シーン遷移完了】 「{}」シーンへ遷移し、初期化しました。", ConvertString(currentSceneName_)));
        Log::Write(L"========================================= [シーン切り替え完了] =========================================");

        // 新しいシーンの初期化
        currentScene_->Initialize();

        // 新シーンのカメラ・ライトの定数バッファおよび行列を直ちにGPUへ反映
        if (auto camMgr = CameraResource::GetCameraManager()) {
            camMgr->Update();
        }
        if (auto lightMgr = LightResource::GetLightManager()) {
            lightMgr->Update();
        }
        EffectManager::GetInstance()->UpdateMatrices();

        // ポーズ中でも即座に描画行列が確定するように、新シーンの全オブジェクトの行列・レンダラーを1度更新
        const auto objectsSnapshot = SceneHierarchy::GetInstance()->GetObjects();
        for (auto* obj : objectsSnapshot) {
            if (obj) {
                obj->UpdateEditor();
            }
        }

        // 古いシーンの破棄や新シーンの初期化・定数バッファ転送完了をGPUと同期待機
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }

    // 予約名をクリア
    nextSceneName_.clear();
    return true;
}

void SceneManager::Update() {
    // 画面切り替えトランジションの更新
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;
    constexpr float kMaxValidDeltaTime = 0.5f;
    float deltaTime = kDefaultDeltaTime;
#ifdef _USEIMGUI
    deltaTime = ImGui::GetIO().DeltaTime;
    if (deltaTime <= 0.0f || deltaTime > kMaxValidDeltaTime) {
        deltaTime = kDefaultDeltaTime;
    }
#endif
    SceneTransitionManager::GetInstance()->Update(deltaTime);

    // 未処理のシーン切り替え予約があれば処理
    ProcessPendingSceneChange();

    if (currentScene_) {
        // ロード画面表示中（暗転・Now Loading...表示中）は新シーンの更新処理を一時停止
        if (!SceneTransitionManager::GetInstance()->IsLoading()) {
            currentScene_->Update();
        }
    }
}

void SceneManager::Draw() {
    // 現在のシーンがあれば描画を実行
    if (currentScene_) {
        currentScene_->Draw();
    }

#ifdef _USEIMGUI
    // カメラやライトの 3D ギズモ・視錐台 (Frustum) の可視化
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr) {
        constexpr int kLightingDisabled = 0;
        constexpr Vector4 kCameraGizmoColor = { 0.2f, 0.7f, 1.0f, 1.0f };         // 通常カメラ: シアン
        constexpr Vector4 kSelectedCameraGizmoColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 選択中カメラ: 鮮やかなイエロー
        constexpr Vector4 kLightGizmoColor = { 1.0f, 0.9f, 0.2f, 1.0f };
        constexpr float kNearZ = 0.2f;
        constexpr float kDefaultGizmoFarZ = 3.0f;
        constexpr float kSelectedGizmoFarZ = 12.0f; // 選択中は視界の奥までフラスタムを延長表示

        if (!dbgCameraModel_) {
            dbgCameraModel_ = std::make_unique<CameraFrustumObject>();
            dbgCameraModel_->SetAutoRegisterHierarchy(false);
            dbgCameraModel_->Initialize(kLightingDisabled);
            dbgCameraModel_->GetMaterialData()->color = kCameraGizmoColor;
            SceneHierarchy::GetInstance()->Unregister(dbgCameraModel_.get());
        }
        if (!dbgLightModel_) {
            dbgLightModel_ = std::make_unique<LightGizmoObject>();
            dbgLightModel_->SetAutoRegisterHierarchy(false);
            dbgLightModel_->Initialize(kLightingDisabled);
            dbgLightModel_->SetRadius(0.5f);
            dbgLightModel_->GetMaterialData()->color = kLightGizmoColor;
            SceneHierarchy::GetInstance()->Unregister(dbgLightModel_.get());
        }

        IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
        std::string activeCamName = cameraMgr->GetActiveCameraName();

        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
        for (auto* obj : objects) {
            // 1. 新コンポーネントシステム (GameObject) のチェック
            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                if (auto* camComp = go->GetComponent<CameraComponent>()) {
                    // 現在自身をレンダリングしているアクティブカメラは画面を覆ってしまうためスキップ
                    if (obj->GetName() == activeCamName) {
                        continue;
                    }

                    bool isSelected = (obj == selected);
                    float farZ = isSelected ? kSelectedGizmoFarZ : kDefaultGizmoFarZ;
                    Vector4 gizmoColor = isSelected ? kSelectedCameraGizmoColor : kCameraGizmoColor;
                    dbgCameraModel_->GetMaterialData()->color = gizmoColor;

                    dbgCameraModel_->SetParameters(camComp->GetFov(), camComp->GetAspectRatio(), kNearZ, farZ);
                    dbgCameraModel_->SetPosition(go->GetPosition());
                    dbgCameraModel_->SetRotate(camComp->GetCalculatedRotation());
                    dbgCameraModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                    dbgCameraModel_->Update();
                    dbgCameraModel_->Draw("white");
                }
                if (auto* lightComp = go->GetComponent<LightComponent>()) {
                    dbgLightModel_->SetPosition(go->GetPosition());
                    dbgLightModel_->SetRotate(go->GetRotate());
                    dbgLightModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                    dbgLightModel_->GetMaterialData()->color = lightComp->GetColor();
                    dbgLightModel_->Update();
                    dbgLightModel_->Draw("white");
                }
            }
            // 2. 従来のオブジェクト (BaseCamera, DirectionalLightObject) のチェック (後方互換)
            else if (auto* cam = dynamic_cast<BaseCamera*>(obj)) {
                if (cam == cameraMgr->GetActiveCamera()) continue;
                bool isSelected = (obj == selected);
                float farZ = isSelected ? kSelectedGizmoFarZ : kDefaultGizmoFarZ;
                Vector4 gizmoColor = isSelected ? kSelectedCameraGizmoColor : kCameraGizmoColor;
                dbgCameraModel_->GetMaterialData()->color = gizmoColor;

                dbgCameraModel_->SetParameters(0.45f, 16.0f / 9.0f, kNearZ, farZ);
                dbgCameraModel_->SetPosition(cam->GetPosition());
                dbgCameraModel_->SetRotate(cam->GetCalculatedRotation());
                dbgCameraModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                dbgCameraModel_->Update();
                dbgCameraModel_->Draw("white");
            } else if (auto* light = dynamic_cast<DirectionalLightObject*>(obj)) {
                dbgLightModel_->SetPosition(light->GetPosition());
                dbgLightModel_->SetRotate(light->GetRotate());
                dbgLightModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                dbgLightModel_->GetMaterialData()->color = kLightGizmoColor;
                dbgLightModel_->Update();
                dbgLightModel_->Draw("white");
            }
        }
    }
#endif
}

void SceneManager::Draw2D() {
    // 現在のシーンがあれば2D/UI描画を実行
    if (currentScene_) {
        currentScene_->Draw2D();
    }
}

GameObject* SceneManager::Instantiate(
    const std::string& prefabName,
    const Vector3& position,
    const Vector3& rotation
) {
    if (!currentScene_) return nullptr;

    auto go = PrefabManager::GetInstance().Instantiate(prefabName, position, rotation);
    if (!go) return nullptr;

    GameObject* rawPtr = go.get();

    // カレントシーンへ追加（ヒエラルキー登録も自動で行われる）
    currentScene_->AddGameObject(std::move(go));

    return rawPtr;
}
