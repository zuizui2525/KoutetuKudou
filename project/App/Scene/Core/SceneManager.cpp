#include "App/Scene/Core/SceneManager.h"
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
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <format>

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
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

        // ポーズ中でも即座に描画行列が確定するように、新シーンの全オブジェクトを1度更新
        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
        for (auto* obj : objects) {
            if (obj) {
                obj->Update();
            }
        }
    }

    // 予約名をクリア
    nextSceneName_.clear();
    return true;
}

void SceneManager::Update() {
    // 未処理のシーン切り替え予約があれば処理
    ProcessPendingSceneChange();

    if (currentScene_) {
        currentScene_->Update();
    }
}

void SceneManager::Draw() {
    // 現在のシーンがあれば描画を実行
    if (currentScene_) {
        currentScene_->Draw();
    }

#ifdef _USEIMGUI
    // ポーズ専用 "Editor" カメラが作動中の場合、他のカメラやライトの位置を 3D 可視化する
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && cameraMgr->GetActiveCameraName() == "Editor") {
        constexpr int kLightingDisabled = 0;
        constexpr Vector4 kCameraGizmoColor = { 0.2f, 0.7f, 1.0f, 1.0f };
        constexpr Vector4 kLightGizmoColor = { 1.0f, 0.9f, 0.2f, 1.0f };

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

        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
        for (auto* obj : objects) {
            // 1. 新コンポーネントシステム (GameObject) のチェック
            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                if (auto* camComp = go->GetComponent<CameraComponent>()) {
                    dbgCameraModel_->SetParameters(camComp->GetFov(), camComp->GetAspectRatio(), 0.2f, 3.0f);
                    dbgCameraModel_->SetPosition(go->GetPosition());
                    dbgCameraModel_->SetRotate(go->GetRotate());
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
                dbgCameraModel_->SetParameters(0.45f, 16.0f / 9.0f, 0.2f, 3.0f);
                dbgCameraModel_->SetPosition(cam->GetPosition());
                dbgCameraModel_->SetRotate(cam->GetRotation());
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
