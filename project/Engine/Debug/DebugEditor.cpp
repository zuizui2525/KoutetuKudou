#ifdef _USEIMGUI
#include "Engine/Debug/DebugEditor.h"
#include "Engine/Debug/SceneViewWindow.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Debug/PerformanceMonitorWindow.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/IGameObject.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Game/GameScene.h"
#include "Engine/Base/Log/Log.h"
#include "externals/imgui/imgui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Engine/Debug/Command/CommandHistory.h"
#include "Engine/Debug/Command/CreateGameObjectCommand.h"
#include "Engine/Debug/Command/DeleteGameObjectCommand.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <filesystem>
#include <format>
#include <vector>
#include <algorithm>

namespace {
    const std::string kScenesDirectory = "resources/Scenes";

    enum class CreationType {
        Empty,
        Cube,
        Sphere,
        Pyramid,
        TriangularPyramid,
        Triangle,
        Square,
        Cylinder,
        Cone,
        Ring,
        Hemisphere,
        Model,
        Line3D,
        FlatRing,
        CylinderEffect,
        Text3D,
        Sprite,
        Triangle2D,
        Circle2D,
        Ring2D,
        Line2D,
        Text2D,
        Camera,
        DirectionalLight,
        PointLight,
        SpotLight,
    };

    void SaveCurrentSceneAction(const std::string& currentSceneName) {
        if (currentSceneName.empty() || currentSceneName == "None") {
            Log::Write(L"[Scene] 有効なシーンが開かれていないため、保存をスキップしました。");
            return;
        }

        std::string scenePath = kScenesDirectory + "/" + currentSceneName + ".json";
        
        // SceneHierarchy から GameObject を収集
        const auto& allObjects = SceneHierarchy::GetInstance()->GetObjects();
        std::vector<GameObject*> gameObjects;
        for (auto* obj : allObjects) {
            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                gameObjects.push_back(go);
            }
        }

        bool success = SceneSerializer::SaveScene(scenePath, currentSceneName, gameObjects);
        if (success) {
            Log::Write(std::format(L"[Scene] 現在のシーン「{}」({} 個の GameObject) を「{}」に正常保存しました！",
                ConvertString(currentSceneName), gameObjects.size(), ConvertString(scenePath)));
        } else {
            Log::Write(std::format(L"[Scene] シーン「{}」の保存に失敗しました。",
                ConvertString(currentSceneName)));
        }
    }

    // アプリケーション終了時のセーブ確認ダイアログ
    bool ConfirmExitAndSave(HWND hwnd) {
        std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
        if (currentSceneName.empty() || currentSceneName == "None") {
            return true;
        }

        std::wstring msg = L"現在のシーン「" + ConvertString(currentSceneName) + L"」を保存してから終了しますか？\n\n"
                           L"【はい】: 保存して終了\n"
                           L"【いいえ】: 保存せずに終了\n"
                           L"【キャンセル】: 終了を取り消す";
        int result = MessageBoxW(hwnd, msg.c_str(), L"終了の確認 - ZuizuiEngine", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (result == IDYES) {
            SaveCurrentSceneAction(currentSceneName);
            return true;
        } else if (result == IDNO) {
            return true;
        }
        return false; // キャンセル時は終了を中断
    }

    // 既存シーン用の静的フォールバック配列（シーン解放まで生存）
    static std::vector<std::unique_ptr<GameObject>> sFallbackObjects;

    void CreateNewGameObjectInCurrentScene(CreationType type = CreationType::Empty) {
        std::string defaultName = "GameObject";
        switch (type) {
        case CreationType::Cube: defaultName = "Cube"; break;
        case CreationType::Sphere: defaultName = "Sphere"; break;
        case CreationType::Pyramid: defaultName = "Pyramid"; break;
        case CreationType::TriangularPyramid: defaultName = "TriangularPyramid"; break;
        case CreationType::Triangle: defaultName = "Triangle"; break;
        case CreationType::Square: defaultName = "Square"; break;
        case CreationType::Cylinder: defaultName = "Cylinder"; break;
        case CreationType::Cone: defaultName = "Cone"; break;
        case CreationType::Ring: defaultName = "Ring"; break;
        case CreationType::Hemisphere: defaultName = "Hemisphere"; break;
        case CreationType::Model: defaultName = "Model"; break;
        case CreationType::Line3D: defaultName = "Line3D"; break;
        case CreationType::FlatRing: defaultName = "FlatRing"; break;
        case CreationType::CylinderEffect: defaultName = "CylinderEffect"; break;
        case CreationType::Text3D: defaultName = "Text3D"; break;
        case CreationType::Sprite: defaultName = "Sprite"; break;
        case CreationType::Triangle2D: defaultName = "Triangle2D"; break;
        case CreationType::Circle2D: defaultName = "Circle2D"; break;
        case CreationType::Ring2D: defaultName = "Ring2D"; break;
        case CreationType::Line2D: defaultName = "Line2D"; break;
        case CreationType::Text2D: defaultName = "Text2D"; break;
        case CreationType::Camera: defaultName = "Camera"; break;
        case CreationType::DirectionalLight: defaultName = "DirectionalLight"; break;
        case CreationType::PointLight: defaultName = "PointLight"; break;
        case CreationType::SpotLight: defaultName = "SpotLight"; break;
        default: defaultName = "GameObject"; break;
        }

        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        GameObject* createdObj = nullptr;
        if (auto genericScene = dynamic_cast<GenericScene*>(currentScene)) {
            createdObj = genericScene->CreateGameObject(defaultName);
        } else {
            auto newObj = std::make_unique<GameObject>(defaultName);
            createdObj = newObj.get();
            sFallbackObjects.push_back(std::move(newObj));
        }

        if (createdObj) {
            if (type == CreationType::Sprite || type == CreationType::Triangle2D ||
                type == CreationType::Circle2D || type == CreationType::Ring2D ||
                type == CreationType::Line2D) {
                auto* sr = createdObj->AddComponent<SpriteRendererComponent>();
                if (type == CreationType::Sprite) sr->SetShapeType(SpriteRendererComponent::ShapeType::Sprite);
                else if (type == CreationType::Triangle2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Triangle);
                else if (type == CreationType::Circle2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Circle);
                else if (type == CreationType::Ring2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Ring);
                else if (type == CreationType::Line2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Line);
                createdObj->Ensure2DPosition();
            } else if (type == CreationType::Text2D) {
                createdObj->AddComponent<TextRenderer2DComponent>();
                createdObj->Ensure2DPosition();
            } else if (type == CreationType::Text3D) {
                createdObj->AddComponent<TextRenderer3DComponent>();
                if (auto cameraMgr = CameraResource::GetCameraManager()) {
                    if (auto* activeCam = cameraMgr->GetActiveCamera()) {
                        Vector3 camPos = activeCam->GetPosition();
                        Vector3 camRot = activeCam->GetRotation();
                        Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
                        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
                        constexpr float kMinForwardLength = 0.001f;
                        if (Math::Length(forward) < kMinForwardLength) {
                            forward = { 0.0f, 0.0f, 1.0f };
                        }
                        constexpr float kSpawnDistanceForward = 8.0f;
                        Vector3 spawnPos = {
                            camPos.x + forward.x * kSpawnDistanceForward,
                            camPos.y + forward.y * kSpawnDistanceForward,
                            camPos.z + forward.z * kSpawnDistanceForward
                        };
                        createdObj->SetPosition(spawnPos);
                    }
                }
            } else if (type == CreationType::Camera) {
                createdObj->AddComponent<CameraComponent>();
            } else if (type == CreationType::DirectionalLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Directional);
            } else if (type == CreationType::PointLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Point);
            } else if (type == CreationType::SpotLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Spot);
            } else if (type != CreationType::Empty) {
                auto* mr = createdObj->AddComponent<MeshRendererComponent>();
                switch (type) {
                case CreationType::Cube: mr->SetMeshType(MeshRendererComponent::MeshType::Cube); break;
                case CreationType::Sphere: mr->SetMeshType(MeshRendererComponent::MeshType::Sphere); break;
                case CreationType::Pyramid: mr->SetMeshType(MeshRendererComponent::MeshType::Pyramid); break;
                case CreationType::TriangularPyramid: mr->SetMeshType(MeshRendererComponent::MeshType::TriangularPyramid); break;
                case CreationType::Triangle: mr->SetMeshType(MeshRendererComponent::MeshType::Triangle); break;
                case CreationType::Square: mr->SetMeshType(MeshRendererComponent::MeshType::Square); break;
                case CreationType::Cylinder: mr->SetMeshType(MeshRendererComponent::MeshType::Cylinder); break;
                case CreationType::Cone: mr->SetMeshType(MeshRendererComponent::MeshType::Cone); break;
                case CreationType::Ring: mr->SetMeshType(MeshRendererComponent::MeshType::Ring); break;
                case CreationType::Hemisphere: mr->SetMeshType(MeshRendererComponent::MeshType::Hemisphere); break;
                case CreationType::Model: mr->SetMeshType(MeshRendererComponent::MeshType::Model); break;
                case CreationType::Line3D: mr->SetMeshType(MeshRendererComponent::MeshType::Line); break;
                case CreationType::FlatRing: mr->SetMeshType(MeshRendererComponent::MeshType::FlatRing); break;
                case CreationType::CylinderEffect: mr->SetMeshType(MeshRendererComponent::MeshType::CylinderEffect); break;
                default: break;
                }

                // 3Dオブジェクトをアクティブカメラの正面（マジックナンバー排除: kSpawnDistanceForward）に配置して見失いを防止
                if (auto cameraMgr = CameraResource::GetCameraManager()) {
                    if (auto* activeCam = cameraMgr->GetActiveCamera()) {
                        Vector3 camPos = activeCam->GetPosition();
                        Vector3 camRot = activeCam->GetRotation();
                        Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
                        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
                        constexpr float kMinForwardLength = 0.001f;
                        if (Math::Length(forward) < kMinForwardLength) {
                            forward = { 0.0f, 0.0f, 1.0f };
                        }
                        constexpr float kSpawnDistanceForward = 8.0f;
                        Vector3 spawnPos = {
                            camPos.x + forward.x * kSpawnDistanceForward,
                            camPos.y + forward.y * kSpawnDistanceForward,
                            camPos.z + forward.z * kSpawnDistanceForward
                        };
                        createdObj->SetPosition(spawnPos);
                    }
                }
            }

            SceneHierarchy::GetInstance()->SetSelected(createdObj);
            CommandHistory::GetInstance()->PushCommand(std::make_unique<CreateGameObjectCommand>(createdObj));
            Log::Write(std::format(L"[Hierarchy] 新規 GameObject「{}」を作成しました。",
                ConvertString(createdObj->GetName())));
        }
    }

    void FocusOnSelectedObject() {
        IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
        if (!selected) return;

        Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
        if (auto* go = dynamic_cast<GameObject*>(selected)) {
            targetPos = go->GetPosition();
        } else if (auto* baseCam = dynamic_cast<BaseCamera*>(selected)) {
            targetPos = baseCam->GetPosition();
        }

        auto cameraMgr = CameraResource::GetCameraManager();
        if (!cameraMgr) return;

        BaseCamera* activeCam = cameraMgr->GetActiveCamera();
        if (!activeCam) return;

        // カメラの現在の向きから前方ベクトルを算出
        Vector3 rot = activeCam->GetRotation();
        Matrix4x4 rotMat = Math::MakeRotateMatrix(rot.x, rot.y, rot.z);
        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
        constexpr float kMinForwardLength = 0.001f;
        if (Math::Length(forward) < kMinForwardLength) {
            forward = { 0.0f, 0.0f, 1.0f };
        }

        constexpr float kFocusDistance = 6.0f;
        Vector3 newCamPos = {
            targetPos.x - forward.x * kFocusDistance,
            targetPos.y - forward.y * kFocusDistance,
            targetPos.z - forward.z * kFocusDistance
        };

        activeCam->SetPosition(newCamPos);

        // アクティブカメラが CameraComponent を持つ GameObject の場合はオーナーの位置も同期
        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        std::string activeName = cameraMgr->GetActiveCameraName();
        if (auto genericScene = dynamic_cast<GenericScene*>(currentScene)) {
            for (const auto& obj : genericScene->GetGameObjects()) {
                if (obj && obj->GetName() == activeName && obj->GetComponent<CameraComponent>()) {
                    obj->SetPosition(newCamPos);
                    break;
                }
            }
        }

        Log::Write(std::format(L"[Editor] 選択オブジェクト「{}」にカメラをフォーカスしました (Fキー)。",
            ConvertString(selected->GetName())));
    }

    void DestroyGameObjectInCurrentScene(GameObject* gameObject) {
        if (!gameObject) return;
        std::string name = gameObject->GetName();

        // 削除前に確実に GPU 同期を実行
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }

        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        std::unique_ptr<GameObject> detached;
        if (currentScene) {
            detached = currentScene->DetachGameObject(gameObject);
        }
        if (!detached) {
            SceneHierarchy::GetInstance()->Unregister(gameObject);
            auto it = std::find_if(sFallbackObjects.begin(), sFallbackObjects.end(),
                [gameObject](const std::unique_ptr<GameObject>& ptr) {
                    return ptr.get() == gameObject;
                });
            if (it != sFallbackObjects.end()) {
                detached = std::move(*it);
                sFallbackObjects.erase(it);
            }
        }

        if (detached) {
            CommandHistory::GetInstance()->PushCommand(std::make_unique<DeleteGameObjectCommand>(std::move(detached)));
        }

        Log::Write(std::format(L"[Hierarchy] GameObject「{}」を削除しました。",
            ConvertString(name)));
    }

    void RenderCreateGameObjectMenuItems() {
        if (ImGui::MenuItem("Create Empty")) {
            CreateNewGameObjectInCurrentScene(CreationType::Empty);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Cube")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cube);
            }
            if (ImGui::MenuItem("Sphere")) {
                CreateNewGameObjectInCurrentScene(CreationType::Sphere);
            }
            if (ImGui::MenuItem("Cylinder")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cylinder);
            }
            if (ImGui::MenuItem("Cone")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cone);
            }
            if (ImGui::MenuItem("Pyramid")) {
                CreateNewGameObjectInCurrentScene(CreationType::Pyramid);
            }
            if (ImGui::MenuItem("Triangular Pyramid")) {
                CreateNewGameObjectInCurrentScene(CreationType::TriangularPyramid);
            }
            if (ImGui::MenuItem("Square (Plane)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Square);
            }
            if (ImGui::MenuItem("Triangle")) {
                CreateNewGameObjectInCurrentScene(CreationType::Triangle);
            }
            if (ImGui::MenuItem("Ring")) {
                CreateNewGameObjectInCurrentScene(CreationType::Ring);
            }
            if (ImGui::MenuItem("Hemisphere")) {
                CreateNewGameObjectInCurrentScene(CreationType::Hemisphere);
            }
            if (ImGui::MenuItem("Line (3D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Line3D);
            }
            if (ImGui::MenuItem("Flat Ring")) {
                CreateNewGameObjectInCurrentScene(CreationType::FlatRing);
            }
            if (ImGui::MenuItem("Cylinder Effect")) {
                CreateNewGameObjectInCurrentScene(CreationType::CylinderEffect);
            }
            if (ImGui::MenuItem("Model")) {
                CreateNewGameObjectInCurrentScene(CreationType::Model);
            }
            if (ImGui::MenuItem("Text (3D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Text3D);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("2D Object")) {
            if (ImGui::MenuItem("Sprite")) {
                CreateNewGameObjectInCurrentScene(CreationType::Sprite);
            }
            if (ImGui::MenuItem("Triangle (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Triangle2D);
            }
            if (ImGui::MenuItem("Circle (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Circle2D);
            }
            if (ImGui::MenuItem("Ring (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Ring2D);
            }
            if (ImGui::MenuItem("Line (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Line2D);
            }
            if (ImGui::MenuItem("Text (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Text2D);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::DirectionalLight);
            }
            if (ImGui::MenuItem("Point Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::PointLight);
            }
            if (ImGui::MenuItem("Spot Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::SpotLight);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Camera")) {
            CreateNewGameObjectInCurrentScene(CreationType::Camera);
        }
    }
}

DebugEditor::DebugEditor()
    : showSceneView_(true),
      showGameView_(true),
      showPerfMonitor_(true),
      showHierarchy_(true),
      showInspector_(true),
      showShortcutsWindow_(false),
      isGameViewVisible_(false),
      isFullscreen_(false),
      currentAspect_(AspectType::Aspect16_9_Low),
      isPaused_(true) {
    wpPrev_.length = sizeof(wpPrev_);
}

DebugEditor::~DebugEditor() {
    sFallbackObjects.clear();
}

void DebugEditor::Initialize() {
    sceneViewWindow_ = std::make_unique<SceneViewWindow>();
    gameViewWindow_ = std::make_unique<GameViewWindow>();
    perfMonitorWindow_ = std::make_unique<PerformanceMonitorWindow>();

    // ウィンドウ終了時（×ボタン、Alt+F4、終了メニュー）のセーブ確認ハンドラを登録
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();
    Zuizui::GetInstance()->GetWindow()->SetCloseHandler([hwnd]() {
        return ConfirmExitAndSave(hwnd);
    });
}

void DebugEditor::Draw(ID3D12GraphicsCommandList* commandList) {
    // 描画開始時に可視性フラグを初期化
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();

    // Ctrl+S ショートカットによる即時シーン保存
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveCurrentSceneAction(SceneManager::GetInstance()->GetCurrentSceneName());
    }

    // Ctrl+Z (Undo) / Ctrl+Y or Ctrl+Shift+Z (Redo) ショートカット (Unity / Blender 準拠)
    if (!io.WantTextInput && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                CommandHistory::GetInstance()->Redo();
            } else {
                CommandHistory::GetInstance()->Undo();
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            CommandHistory::GetInstance()->Redo();
        }
    }

    // F キーによるオブジェクトフォーカス (Focus on Selection)
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        FocusOnSelectedObject();
    }

    // F1 キーによるショートカット一覧の開閉
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        showShortcutsWindow_ = !showShortcutsWindow_;
    }

    // メインメニューバーの描画
    DrawMenuBar(hwnd);

    // ドックスペースの設定
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Scene View (編集画面)
    if (showSceneView_) {
        sceneViewWindow_->Draw(&showSceneView_);
    }

    // Game View (ゲーム画面)
    if (showGameView_) {
        gameViewWindow_->Draw(&showGameView_, &isGameViewVisible_);
    } else {
        isGameViewVisible_ = false;
    }

    // Console
    Log::DrawConsoleWindow(-1.0f);

    // Performance Monitor
    if (showPerfMonitor_) {
        perfMonitorWindow_->Draw(&showPerfMonitor_);
    }

    // Hierarchy (左側)
    if (showHierarchy_) {
        if (ImGui::Begin("ヒエラルキー###Hierarchy", &showHierarchy_)) {
            // [+ Create GameObject] ボタン
            constexpr float kCreateBtnHeight = 24.0f;
            if (ImGui::Button("+ Create GameObject", ImVec2(-1.0f, kCreateBtnHeight))) {
                ImGui::OpenPopup("CreateGameObjectPopup");
            }

            if (ImGui::BeginPopup("CreateGameObjectPopup")) {
                RenderCreateGameObjectMenuItems();
                ImGui::EndPopup();
            }
            ImGui::Separator();

            const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            GameObject* objToDelete = nullptr;

            for (size_t i = 0; i < objects.size(); ++i) {
                auto* obj = objects[i];
                ImGui::PushID(static_cast<int>(i));

                // 表示フラグ用のチェックボックス
                bool isVisible = obj->IsVisible();
                std::string chkLabel = "##visible_" + obj->GetName();
                if (ImGui::Checkbox(chkLabel.c_str(), &isVisible)) {
                    obj->SetVisible(isVisible);
                }
                ImGui::SameLine();

                auto* go = dynamic_cast<GameObject*>(obj);
                bool isSelected = (obj == selected);

                if (go) {
                    // 枠線や四角い背景を一切持たないシンプルな「x」テキストUI (マジックナンバー排除)
                    constexpr float kDeleteBtnWidth = 14.0f;
                    constexpr float kItemSpacing = 4.0f;
                    constexpr float kMinSelectableWidth = 50.0f;
                    const ImVec4 kNormalTextColor = { 0.5f, 0.5f, 0.5f, 0.7f };  // 通常時: 控えめなグレー
                    const ImVec4 kHoveredTextColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // ホバー時: 白くハイライト（赤色は完全排除）

                    float availWidth = ImGui::GetContentRegionAvail().x;
                    float selectableWidth = (std::max)(kMinSelectableWidth, availWidth - kDeleteBtnWidth - kItemSpacing);

                    // 現在アクティブなカメラかどうか判定
                    bool isActiveCam = false;
                    if (auto cameraMgr = CameraResource::GetCameraManager()) {
                        if (go->GetComponent<CameraComponent>() && obj->GetName() == cameraMgr->GetActiveCameraName()) {
                            isActiveCam = true;
                        }
                    }

                    std::string label = obj->GetName();
                    if (isActiveCam) {
                        label += " [Active]";
                        constexpr ImVec4 kActiveCamColor = { 0.35f, 0.85f, 1.0f, 1.0f };
                        ImGui::PushStyleColor(ImGuiCol_Text, kActiveCamColor);
                    }

                    ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(selectableWidth, 0.0f));

                    if (isActiveCam) {
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        FocusOnSelectedObject();
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpTranslate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpRotate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpScale);
                    }

                    ImGui::SameLine();
                    // 四角い枠や背景を描画しない不可視ボタンでクリックとホバーを検知
                    ImVec2 btnPos = ImGui::GetCursorPos();
                    if (ImGui::InvisibleButton("##del_btn", ImVec2(kDeleteBtnWidth, ImGui::GetTextLineHeight()))) {
                        objToDelete = go;
                    }
                    bool isBtnHovered = ImGui::IsItemHovered();

                    // 同じ位置にテキスト単体を描画（ホバー時のみ上品に白く点灯）
                    ImGui::SetCursorPos(btnPos);
                    ImGui::TextColored(isBtnHovered ? kHoveredTextColor : kNormalTextColor, "x");
                } else {
                    ImGui::Selectable(obj->GetName().c_str(), isSelected);
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpTranslate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpRotate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpScale);
                    }
                }

                ImGui::PopID();
            }

            if (objToDelete) {
                DestroyGameObjectInCurrentScene(objToDelete);
            }

            // 空白エリアの右クリックコンテキストメニュー
            if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                RenderCreateGameObjectMenuItems();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    // Inspector (右側)
    if (showInspector_) {
        if (ImGui::Begin("インスペクター###Inspector", &showInspector_)) {
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            if (selected) {
                // 名前の編集
                constexpr int kNameBufferSize = 128;
                char nameBuf[kNameBufferSize];
                strcpy_s(nameBuf, selected->GetName().c_str());
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    selected->SetName(nameBuf);
                }

                ImGui::Separator();

                // 各種オブジェクト固有のインスペクター描画
                selected->DrawInspector();
            } else {
                ImGui::Text("オブジェクトが選択されていません。");
            }
        }
        ImGui::End();
    }

    // ショートカットキー一覧ダイアログ
    if (showShortcutsWindow_) {
        DrawShortcutsWindow();
    }
}

void DebugEditor::DrawMenuBar(HWND hwnd) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text("ZuizuiEngine");
        ImGui::Separator();
        
        if (ImGui::BeginMenu("ファイル###File")) {
            if (ImGui::MenuItem("保存", "Ctrl+S")) {
                std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
                SaveCurrentSceneAction(currentSceneName);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("終了", "Alt+F4")) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("編集###Edit")) {
            auto cmdHistory = CommandHistory::GetInstance();
            std::string undoLabel = "元に戻す (Undo)";
            if (cmdHistory->CanUndo()) {
                undoLabel += " - " + cmdHistory->GetUndoName();
            }
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, cmdHistory->CanUndo())) {
                cmdHistory->Undo();
            }

            std::string redoLabel = "やり直す (Redo)";
            if (cmdHistory->CanRedo()) {
                redoLabel += " - " + cmdHistory->GetRedoName();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, cmdHistory->CanRedo())) {
                cmdHistory->Redo();
            }
            ImGui::EndMenu();
        }

        // ----------------------------------------------------
        // シーン管理メニュー (Scene) - 友達のエンジン完全準拠
        // ----------------------------------------------------
        if (ImGui::BeginMenu("Scene###SceneMenu")) {
            std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();

            // 1. シーン一覧（resources/Scenes/ 内の .json ファイルを走査）
            if (!std::filesystem::exists(kScenesDirectory)) {
                std::filesystem::create_directories(kScenesDirectory);
            }

            std::vector<std::string> sceneNames;
            for (const auto& entry : std::filesystem::directory_iterator(kScenesDirectory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string stemName = entry.path().stem().string();
                    sceneNames.push_back(stemName);
                }
            }
            // 既存の基本シーンも一覧に追加（重複排除）
            const std::vector<std::string> defaultScenes = { "Sample", "Title", "Game", "Clear", "GameOver", "Debug" };
            for (const auto& ds : defaultScenes) {
                if (std::find(sceneNames.begin(), sceneNames.end(), ds) == sceneNames.end()) {
                    sceneNames.push_back(ds);
                }
            }

            // シーン一覧表示
            for (const auto& sName : sceneNames) {
                bool isSelected = (sName == currentSceneName);
                if (ImGui::MenuItem(sName.c_str(), nullptr, isSelected)) {
                    if (!isSelected) {
                        // 切り替え前に現在のシーンを自動保存！
                        SaveCurrentSceneAction(currentSceneName);
                        SceneManager::GetInstance()->ChangeScene(sName);
                    }
                }
            }

            ImGui::Separator();

            // 2. 新規シーン作成 (New Scene)
            static char newSceneNameBuf[64] = "";
            constexpr float kInputWidth = 110.0f;
            ImGui::SetNextItemWidth(kInputWidth);
            bool enterPressed = ImGui::InputTextWithHint("##NewSceneInput", "Scene name...", newSceneNameBuf, sizeof(newSceneNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            bool newBtnClicked = ImGui::Button("New Scene");

            if ((enterPressed || newBtnClicked) && strlen(newSceneNameBuf) > 0) {
                // 新規シーンへ切り替える前に現在のシーンを自動保存！
                SaveCurrentSceneAction(currentSceneName);

                std::string newName = newSceneNameBuf;
                std::string newPath = kScenesDirectory + "/" + newName + ".json";

                // 空シーンのJSONを作成
                std::vector<std::unique_ptr<GameObject>> emptyObjects;
                SceneSerializer::SaveScene(newPath, newName, emptyObjects);

                // 新シーンへ切り替え
                SceneManager::GetInstance()->ChangeScene(newName);

                newSceneNameBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();

            // 3. 現在のシーン保存 (Save Current Scene)
            if (ImGui::MenuItem("Save Current Scene", "Ctrl+S")) {
                SaveCurrentSceneAction(currentSceneName);
            }

            // 4. シーン削除 (Delete Scene)
            if (ImGui::BeginMenu("Delete Scene")) {
                bool hasDeletable = false;
                for (const auto& sName : sceneNames) {
                    if (sName != currentSceneName) {
                        std::string pathToCheck = kScenesDirectory + "/" + sName + ".json";
                        if (std::filesystem::exists(pathToCheck)) {
                            hasDeletable = true;
                            if (ImGui::MenuItem(sName.c_str())) {
                                std::filesystem::remove(pathToCheck);
                                Log::Write(std::format(L"[Scene] シーンファイル「{}」を削除しました。", ConvertString(pathToCheck)));
                            }
                        }
                    }
                }
                if (!hasDeletable) {
                    ImGui::TextDisabled("削除可能な保存シーンがありません");
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("表示###View")) {
            ImGui::MenuItem("シーン (編集)", nullptr, &showSceneView_);
            ImGui::MenuItem("ゲーム画面", nullptr, &showGameView_);
            ImGui::MenuItem("ヒエラルキー", nullptr, &showHierarchy_);
            ImGui::MenuItem("インスペクター", nullptr, &showInspector_);
            ImGui::MenuItem("コンソール", nullptr, Log::GetShowConsolePtr());
            ImGui::MenuItem("カメラ一覧", nullptr, CameraManager::GetShowWindowPtr());
            ImGui::MenuItem("ポストエフェクト", nullptr, PostProcess::GetShowWindowPtr());
            ImGui::MenuItem("エフェクト一覧", nullptr, EffectManager::GetShowListWindowPtr());
            ImGui::MenuItem("パフォーマンス監視", nullptr, &showPerfMonitor_);
            
            GameScene* gameScene = dynamic_cast<GameScene*>(SceneManager::GetInstance()->GetCurrentScene());
            if (gameScene) {
                ImGui::Separator();
                ImGui::MenuItem("ルートエディタ", nullptr, gameScene->GetShowRouteEditorPtr());
                ImGui::MenuItem("敵エディタ", nullptr, gameScene->GetShowStageEditorPtr());
            }

            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("ウィンドウ###Window")) {
            if (ImGui::MenuItem("フルスクリーン", "F11", &isFullscreen_)) {
                DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
                
                // 元のサイズ変更不可のウィンドウスタイルをローカル定数定義（マジックナンバー排除）
                const DWORD kOriginalStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

                if (isFullscreen_) {
                    // フルスクリーン化
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetWindowPlacement(hwnd, &wpPrev_) &&
                        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                        SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                        SetWindowPos(hwnd, HWND_TOP,
                                     mi.rcMonitor.left, mi.rcMonitor.top,
                                     mi.rcMonitor.right - mi.rcMonitor.left,
                                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                    }
                } else {
                    // 元の画面サイズに戻す
                    SetWindowLong(hwnd, GWL_STYLE, kOriginalStyle);
                    
                    wpPrev_.showCmd = SW_SHOWNORMAL;
                    
                    // 直前のアスペクト比設定に応じたクライアント解像度を決定（マジックナンバー排除）
                    int32_t restoreWidth = 1280;
                    int32_t restoreHeight = 720;
                    switch (currentAspect_) {
                    case AspectType::Aspect16_9_Low:
                        restoreWidth = 1280;
                        restoreHeight = 720;
                        break;
                    case AspectType::Aspect16_9_High:
                        restoreWidth = 1920;
                        restoreHeight = 1080;
                        break;
                    case AspectType::Aspect4_3:
                        restoreWidth = 960;
                        restoreHeight = 720;
                        break;
                    case AspectType::Aspect1_1:
                        restoreWidth = 720;
                        restoreHeight = 720;
                        break;
                    }

                    RECT wr = { 0, 0, restoreWidth, restoreHeight };
                    AdjustWindowRect(&wr, kOriginalStyle, FALSE);
                    
                    wpPrev_.rcNormalPosition.right = wpPrev_.rcNormalPosition.left + (wr.right - wr.left);
                    wpPrev_.rcNormalPosition.bottom = wpPrev_.rcNormalPosition.top + (wr.bottom - wr.top);
                    
                    SetWindowPlacement(hwnd, &wpPrev_);
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                }
            }
            
            ImGui::Separator();

            // アスペクト比変更メニュー
            if (!isFullscreen_ && ImGui::BeginMenu("アスペクト比###Aspect Ratio")) {
                const DWORD kOriginalStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
                
                auto ChangeWindowSize = [&](int32_t width, int32_t height) {
                    RECT wr = { 0, 0, width, height };
                    AdjustWindowRect(&wr, kOriginalStyle, FALSE);
                    
                    RECT rect{};
                    GetWindowRect(hwnd, &rect);
                    
                    SetWindowPos(hwnd, NULL,
                                 rect.left, rect.top,
                                 wr.right - wr.left,
                                 wr.bottom - wr.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                };

                if (ImGui::MenuItem("16:9 (1280x720)", nullptr, currentAspect_ == AspectType::Aspect16_9_Low)) {
                    currentAspect_ = AspectType::Aspect16_9_Low;
                    ChangeWindowSize(1280, 720);
                }
                if (ImGui::MenuItem("16:9 (1920x1080)", nullptr, currentAspect_ == AspectType::Aspect16_9_High)) {
                    currentAspect_ = AspectType::Aspect16_9_High;
                    ChangeWindowSize(1920, 1080);
                }
                if (ImGui::MenuItem("4:3 (960x720)", nullptr, currentAspect_ == AspectType::Aspect4_3)) {
                    currentAspect_ = AspectType::Aspect4_3;
                    ChangeWindowSize(960, 720);
                }
                if (ImGui::MenuItem("1:1 (720x720)", nullptr, currentAspect_ == AspectType::Aspect1_1)) {
                    currentAspect_ = AspectType::Aspect1_1;
                    ChangeWindowSize(720, 720);
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("最大化")) {
                ShowWindow(hwnd, SW_MAXIMIZE);
            }
            if (ImGui::MenuItem("最小化")) {
                ShowWindow(hwnd, SW_MINIMIZE);
            }
            if (ImGui::MenuItem("元のサイズに戻す")) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ヘルプ###Help")) {
            if (ImGui::MenuItem("ショートカットキー一覧 (Shortcuts)", "F1", &showShortcutsWindow_)) {
            }
            ImGui::EndMenu();
        }

        // 画面中央に再生/一時停止ボタンを配置（固定幅で完全中央揃え）
        float menuBarWidth = ImGui::GetWindowWidth();
        constexpr float kPlayPauseBtnWidth = 85.0f;
        float centerPos = (menuBarWidth - kPlayPauseBtnWidth) * 0.5f;
        ImGui::SameLine(centerPos);

        if (isPaused_) {
            if (ImGui::Button("再生 ▶", ImVec2(kPlayPauseBtnWidth, 0.0f))) {
                isPaused_ = false;
                if (gameViewWindow_) {
                    gameViewWindow_->TriggerPopAnimation(PopAnimation::Type::Play);
                }
            }
        } else {
            if (ImGui::Button("一時停止 ||", ImVec2(kPlayPauseBtnWidth, 0.0f))) {
                isPaused_ = true;
                if (gameViewWindow_) {
                    gameViewWindow_->TriggerPopAnimation(PopAnimation::Type::Pause);
                }
            }
        }

        ImGui::EndMainMenuBar();
    }
}

void DebugEditor::DrawShortcutsWindow() {
    constexpr float kDefaultWindowWidth = 540.0f;
    constexpr float kDefaultWindowHeight = 520.0f;
    ImGui::SetNextWindowSize(ImVec2(kDefaultWindowWidth, kDefaultWindowHeight), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("ショートカットキー一覧###ShortcutsWindow", &showShortcutsWindow_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextDisabled("ZuizuiEngine で使用できるキーボード＆マウスショートカットの一覧です。");
        ImGui::Separator();

        auto DrawShortcutTable = [](const char* tableId, const std::vector<std::pair<std::string, std::string>>& items) {
            constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | 
                                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
            constexpr float kColWidthKey = 0.38f;
            constexpr float kColWidthDesc = 0.62f;

            if (ImGui::BeginTable(tableId, 2, kTableFlags)) {
                ImGui::TableSetupColumn("操作 / キー", ImGuiTableColumnFlags_WidthStretch, kColWidthKey);
                ImGui::TableSetupColumn("機能・説明", ImGuiTableColumnFlags_WidthStretch, kColWidthDesc);
                ImGui::TableHeadersRow();

                for (const auto& [key, desc] : items) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    constexpr ImVec4 kKeyColor = { 0.4f, 0.85f, 1.0f, 1.0f }; // シアン系の強調色
                    ImGui::TextColored(kKeyColor, "%s", key.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(desc.c_str());
                }
                ImGui::EndTable();
            }
        };

        if (ImGui::CollapsingHeader("一般操作 (General)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const std::vector<std::pair<std::string, std::string>> generalShortcuts = {
                { "Ctrl + S", "現在のシーンを保存 (Save Current Scene)" },
                { "Ctrl + Z", "直前の操作を元に戻す (Undo)" },
                { "Ctrl + Y / Ctrl + Shift + Z", "取り消した操作をやり直す (Redo)" },
                { "F", "選択中のオブジェクトへカメラをフォーカス" },
                { "F1", "このショートカットキー一覧ウィンドウの開閉" },
                { "F11", "フルスクリーン / ウィンドウモードの切り替え" },
                { "Alt + F4", "アプリケーションの終了（保存確認あり）" }
            };
            DrawShortcutTable("GeneralTable", generalShortcuts);
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("シーン・カメラ操作 (Scene View & Camera)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const std::vector<std::pair<std::string, std::string>> cameraShortcuts = {
                { "右ドラッグ (Right Drag)", "カメラの視線回転（Look Around / FPS視点）" },
                { "右ドラッグ + W / S", "カメラの前進 / 後退移動" },
                { "右ドラッグ + A / D", "カメラの左 / 右平行移動" },
                { "右ドラッグ + E / Q", "カメラの上昇 / 下降移動" },
                { "マウスホイール回転", "前後ズーム (Zoom In / Out)" },
                { "中クリックドラッグ", "カメラの平行移動 (Pan)" }
            };
            DrawShortcutTable("CameraTable", cameraShortcuts);
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("ギズモ・オブジェクト操作 (Gizmo & Manipulation)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const std::vector<std::pair<std::string, std::string>> gizmoShortcuts = {
                { "ヒエラルキー 左クリック", "平行移動ギズモ (Translate) で選択" },
                { "ヒエラルキー 中クリック", "回転ギズモ (Rotate) で選択" },
                { "ヒエラルキー 右クリック", "拡縮ギズモ (Scale) で選択" },
                { "ヒエラルキー ダブルクリック", "オブジェクトを選択してカメラフォーカス (F)" },
                { "ギズモ軸ドラッグ", "選択オブジェクトのTransform操作 (Undo/Redo対応)" },
                { "ヒエラルキー [x] ボタン", "オブジェクトの削除 (Undo/Redo対応)" }
            };
            DrawShortcutTable("GizmoTable", gizmoShortcuts);
        }
    }
    ImGui::End();
}
#endif

