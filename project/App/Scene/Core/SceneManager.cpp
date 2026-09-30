#include "App/Scene/Core/SceneManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Graphics/Objects/3d/Pyramid/PyramidObject.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"
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

void SceneManager::Update() {
    // 次のシーン名が入っていたら切り替え処理を行う
    if (!nextSceneName_.empty()) {

        // 工場がセットされていない場合はエラー（または早期リターン）
        if (!sceneFactory_) return;

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
        }

        // 予約名をクリア
        nextSceneName_.clear();
    }

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
        if (!dbgCameraModel_) {
            dbgCameraModel_ = std::make_unique<PyramidObject>();
            dbgCameraModel_->Initialize(0); // ライティング無効
            dbgCameraModel_->GetMaterialData()->color = { 0.2f, 0.6f, 1.0f, 1.0f }; // 青系の色
        }
        if (!dbgLightModel_) {
            dbgLightModel_ = std::make_unique<SphereObject>();
            dbgLightModel_->Initialize(0); // ライティング無効
            dbgLightModel_->GetMaterialData()->color = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄色
        }

        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
        for (auto* obj : objects) {
            if (auto* cam = dynamic_cast<BaseCamera*>(obj)) {
                // 自分自身（Editor カメラ）は描画しない
                if (cam == cameraMgr->GetActiveCamera()) continue;

                // カメラの位置に四角錐モデルを描画
                dbgCameraModel_->SetPosition(cam->GetPosition());
                dbgCameraModel_->SetRotate(cam->GetRotation());
                dbgCameraModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                dbgCameraModel_->Update();
                dbgCameraModel_->Draw("white");
            } else if (auto* light = dynamic_cast<DirectionalLightObject*>(obj)) {
                // ライトの位置に球体モデルを描画
                dbgLightModel_->SetPosition(light->GetPosition());
                dbgLightModel_->SetRotate(light->GetRotate());
                dbgLightModel_->SetScale({ 1.0f, 1.0f, 1.0f });
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
