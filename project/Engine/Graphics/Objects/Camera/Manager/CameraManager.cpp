#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <format>

void CameraManager::Initialize() {
    // Engine
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    // --- 1. 2D用行列の初期化 (固定) ---
    viewMatrix2D_ = Math::MakeIdentity();
    projectionMatrix2D_ = Math::MakeOrthographicMatrix(
        0.0f, 0.0f,
        static_cast<float>(WindowApp::kClientWidth),
        static_cast<float>(WindowApp::kClientHeight),
        0.0f, 100.0f
    );

    // --- 2. GPU転送用リソースの作成 ---
    // CameraForGPU構造体のサイズでバッファを確保
    resource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(CameraForGPU));
    // 常時マッピングしておく
    resource_->Map(0, nullptr, reinterpret_cast<void**>(&data_));

    Log::Write(L" ├─ 【カメラ用バッファ初期化】 GPU転送用カメラ定数バッファの確保・マッピングに成功しました。");

    // --- 3. ポーズ専用 Editor カメラの自動生成と登録 ---
    auto editorCam = std::make_shared<DebugCamera>();
    editorCam->Initialize();
    editorCam->SetPosition({ 0.0f, 10.0f, -20.0f });
    editorCam->SetRotation({ 0.4f, 0.0f, 0.0f });
    AddCamera("Editor", editorCam);
}

void CameraManager::Update() {
    // アクティブなカメラがなければ何もしない
    if (!activeCamera_) return;

    // --- 3. 3Dカメラ(アクティブ)の固有更新 ---
    // ここで各カメラクラス(DebugCamera等)の行列計算が行われる
    activeCamera_->Update();

    // --- 4. GPU定数バッファへのデータ転送 ---
    // 定数バッファは、常に「現在アクティブなカメラ」の情報を指すように上書き
    data_->view = activeCamera_->GetViewMatrix();
    data_->projection = activeCamera_->GetProjectionMatrix();
    data_->worldPosition = activeCamera_->GetPosition();
}

void CameraManager::ImGuiControl() {
#ifdef _USEIMGUI
    if (!showWindow_) {
        return;
    }

    if (ImGui::Begin("カメラ一覧###Camera List", &showWindow_)) {
        for (auto& [name, camera] : cameras_) {
            bool isActive = (activeCamera_ == camera.get());

            // 1. ラベルを作成 (例: "MainCamera (アクティブ)")
            std::string label = name + (isActive ? " (アクティブ)" : "");

            // 2. ラジオボタンを縦に並べる (SameLineを呼ばない)
            // ##name をつけることで、内部IDを固定し、表示文字列が変わっても挙動を安定させる
            if (ImGui::RadioButton((label + "##" + name).c_str(), isActive)) {
                SetActiveCamera(name);
            }

            ImGui::Separator(); // カメラごとに区切り線を入れると見やすい
        }
    }
    ImGui::End();
#endif
}

void CameraManager::Clear() {
    // "Editor" カメラを一時的に退避させる
    std::shared_ptr<BaseCamera> editorCam = nullptr;
    auto it = cameras_.find("Editor");
    if (it != cameras_.end()) {
        editorCam = it->second;
    }

    if (cameras_.size() > (editorCam ? 1 : 0)) {
        Log::Write(L" ├─ 【カメラシステムクリア】 登録されていたすべてのカメラリソースを破棄しました。");
    }
    cameras_.clear();
    activeCamera_ = nullptr;

    // 退避させた "Editor" を再格納して温存する
    if (editorCam) {
        cameras_["Editor"] = editorCam;
    }
}

void CameraManager::AddCamera(const std::string& name, std::shared_ptr<BaseCamera> camera) {
    cameras_[name] = camera;
 
    auto pos = camera->GetPosition();
    Log::Write(std::format(L" ├─ 【カメラ登録成功】 名前:「{}」 | 初期座標: ({:.2f}, {:.2f}, {:.2f})", 
        ConvertString(name), pos.x, pos.y, pos.z));
 
    // 最初の登録、または仮に "Editor" が設定されている状態から新しいゲームカメラが登録された場合は上書きする
    bool isEditor = (name == "Editor");
    std::string currentActiveName = GetActiveCameraName();

    if (!activeCamera_ || (currentActiveName == "Editor" && !isEditor)) {
        activeCamera_ = camera.get();
        Log::Write(std::format(L" ├─ 【アクティブカメラ設定】 「{}」カメラを起動用カメラに設定しました。", ConvertString(name)));
    }
}
 
void CameraManager::SetActiveCamera(const std::string& name, bool log) {
    // 既にそのカメラがアクティブなら何もしない
    if (GetActiveCameraName() == name) return;

    // 指定された名前のカメラがマップに存在するか確認
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        activeCamera_ = it->second.get();
        if (log) {
            Log::Write(std::format(L" ├─ 【アクティブカメラ切替】 「{}」カメラがアクティブになりました。", ConvertString(name)));
        }
    }
}

void CameraManager::UpdateAllProjection(float aspect) {
    for (auto& [name, camera] : cameras_) {
        if (camera) {
            camera->UpdateProjection(aspect);
        }
    }
}

