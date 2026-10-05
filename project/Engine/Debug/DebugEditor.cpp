#ifdef _USEIMGUI
#include "Engine/Debug/DebugEditor.h"
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
#include "App/Scene/Core/SceneSerializer.h"
#include "App/Scene/Generic/GenericScene.h"
#include "App/Scene/Title/TitleScene.h"
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
        Sprite,
        Triangle2D,
        Circle2D,
        Ring2D,
        Line2D,
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
        case CreationType::Sprite: defaultName = "Sprite"; break;
        case CreationType::Triangle2D: defaultName = "Triangle2D"; break;
        case CreationType::Circle2D: defaultName = "Circle2D"; break;
        case CreationType::Ring2D: defaultName = "Ring2D"; break;
        case CreationType::Line2D: defaultName = "Line2D"; break;
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
        } else if (auto titleScene = dynamic_cast<TitleScene*>(currentScene)) {
            createdObj = titleScene->CreateGameObject(defaultName);
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
            }

            SceneHierarchy::GetInstance()->SetSelected(createdObj);
            Log::Write(std::format(L"[Hierarchy] 新規 GameObject「{}」を作成しました。",
                ConvertString(createdObj->GetName())));
        }
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
        if (auto genericScene = dynamic_cast<GenericScene*>(currentScene)) {
            genericScene->DestroyGameObject(gameObject);
        } else if (auto titleScene = dynamic_cast<TitleScene*>(currentScene)) {
            titleScene->DestroyGameObject(gameObject);
        } else {
            SceneHierarchy::GetInstance()->Unregister(gameObject);
            auto it = std::remove_if(sFallbackObjects.begin(), sFallbackObjects.end(),
                [gameObject](const std::unique_ptr<GameObject>& ptr) {
                    return ptr.get() == gameObject;
                });
            if (it != sFallbackObjects.end()) {
                sFallbackObjects.erase(it, sFallbackObjects.end());
            }
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
    : showGameView_(true),
      showPerfMonitor_(true),
      showHierarchy_(true),
      showInspector_(true),
      isGameViewVisible_(false),
      isFullscreen_(false),
      currentAspect_(AspectType::Aspect16_9_Low),
      isPaused_(false) {
    wpPrev_.length = sizeof(wpPrev_);
}

DebugEditor::~DebugEditor() = default;

void DebugEditor::Initialize() {
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

    // メインメニューバーの描画
    DrawMenuBar(hwnd);

    // ドックスペースの設定
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Game View
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

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        gameViewWindow_->SetGizmoOperation(7); // TRANSLATE
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        gameViewWindow_->SetGizmoOperation(120); // ROTATE
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        gameViewWindow_->SetGizmoOperation(896); // SCALE
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
                        gameViewWindow_->SetGizmoOperation(7); // TRANSLATE
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        gameViewWindow_->SetGizmoOperation(120); // ROTATE
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        gameViewWindow_->SetGizmoOperation(896); // SCALE
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
            // 既存のハードコードシーンも一覧に追加（重複排除）
            const std::vector<std::string> defaultScenes = { "Sample", "Title", "Game", "Debug" };
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

        if (ImGui::BeginMenu("設定###Settings")) {
            if (gameViewWindow_) {
                // 1. クリック一時停止チェックボックス & 説明
                bool enable = gameViewWindow_->IsClickPauseEnabled();
                if (ImGui::Checkbox("クリック一時停止", &enable)) {
                    gameViewWindow_->SetClickPauseEnabled(enable);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("ゲーム画面内を左クリックした際に、ゲームの一時停止／再開を切り替えます");
                }
                ImGui::TextDisabled("  画面クリックで一時停止/再開");

                // 2. ギズモ操作モード（現在のモードを表示）
                int currentOp = gameViewWindow_->GetGizmoOperation();
                const char* currentOpLabel = "移動";
                if (currentOp == GameViewWindow::kGizmoOpRotate) {
                    currentOpLabel = "回転";
                } else if (currentOp == GameViewWindow::kGizmoOpScale) {
                    currentOpLabel = "拡縮";
                }

                constexpr int kMenuTitleBufferSize = 64;
                char menuTitle[kMenuTitleBufferSize];
                sprintf_s(menuTitle, "ギズモ操作モード [%s]", currentOpLabel);

                if (ImGui::BeginMenu(menuTitle)) {
                    if (ImGui::MenuItem("移動 (Translate)", nullptr, currentOp == GameViewWindow::kGizmoOpTranslate)) {
                        gameViewWindow_->SetGizmoOperation(GameViewWindow::kGizmoOpTranslate);
                    }
                    if (ImGui::MenuItem("回転 (Rotate)", nullptr, currentOp == GameViewWindow::kGizmoOpRotate)) {
                        gameViewWindow_->SetGizmoOperation(GameViewWindow::kGizmoOpRotate);
                    }
                    if (ImGui::MenuItem("拡縮 (Scale)", nullptr, currentOp == GameViewWindow::kGizmoOpScale)) {
                        gameViewWindow_->SetGizmoOperation(GameViewWindow::kGizmoOpScale);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("3Dギズモの操作種別（平行移動・回転・拡大縮小）を選択します");
                }
            }
            ImGui::EndMenu();
        }

        // 画面中央付近に再生/一時停止ボタンを配置
        float menuBarWidth = ImGui::GetWindowWidth();
        constexpr float kPlayPauseBtnWidth = 70.0f;
        float centerPos = (menuBarWidth - kPlayPauseBtnWidth) * 0.5f;
        ImGui::SameLine(centerPos);

        if (isPaused_) {
            if (ImGui::Button("再生 ▶")) {
                isPaused_ = false;
            }
        } else {
            if (ImGui::Button("一時停止 ||")) {
                isPaused_ = true;
            }
        }

        ImGui::EndMainMenuBar();
    }
}
#endif
