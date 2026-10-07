#include "App/App.h"
#include "Engine/Debug/DebugEditor.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/SceneFactory.h"
#include "App/Load/ResourceLoader.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Base/Log/Log.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Debug/Command/CommandHistory.h"
#include "Engine/Debug/SceneViewWindow.h"
#include "Engine/Debug/GameViewWindow.h"
#include <psapi.h> // メモリ取得用（追加）


#pragma comment(lib, "psapi.lib") // 追加


void App::Initialize() {
    // システム
    engine_ = Zuizui::GetInstance();
    EngineResource::SetEngine(engine_);
#ifdef _USEIMGUI
    engine_->Initialize(L"ZuizuiEngine");
#else
    engine_->Initialize(L"LE3B_02_イトウカズイ");
#endif

    input_ = std::make_unique<Input>();
    input_->Initialize();
    InputResource::SetInput(input_.get());

    cameraMgr_ = std::make_unique<CameraManager>();
    cameraMgr_->Initialize();
    CameraResource::SetCameraManager(cameraMgr_.get());

    lightMgr_ = std::make_unique<LightManager>();
    lightMgr_->Initialize();
    LightResource::SetLightManager(lightMgr_.get());

    texMgr_ = std::make_unique<TextureManager>();
    texMgr_->Initialize();
    TextureResource::SetTextureManager(texMgr_.get());

    modelMgr_ = std::make_unique<ModelManager>();
    modelMgr_->Initialize();
    ModelResource::SetModelManager(modelMgr_.get());

    // リソースの一括ロード
    ResourceLoader::LoadAll();

    sceneFactory_ = std::make_unique<SceneFactory>();
    SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

    static const std::string kDefaultInitialSceneName = "Title";
    SceneManager::GetInstance()->ChangeScene(kDefaultInitialSceneName);

    // --- PostProcess の初期化 ---
    postProcess_ = std::make_unique<PostProcess>();
    postProcess_->Initialize();

    SceneManager::GetInstance()->SetPostProcess(postProcess_.get());

    // [Task 2-4] シーンシリアライザの動的コンポーネント着脱・保存・復元の単体テスト
    bool selfTestPassed = SceneSerializer::RunSelfTest();
    Log::Write(selfTestPassed 
        ? L" ├─ 【単体テスト成功】 SceneSerializer 動的コンポーネント保存・復元テストに合格しました。"
        : L" ├─ 【単体テスト失敗】 SceneSerializer のテストに失敗しました。");
}

void App::Run() {
    // メインループの最先頭でフレームタイマーの測定を開始（計測漏れを防止）
    engine_->GetDxCommon()->FrameStart();

    // 現在のウィンドウの実際のクライアント領域サイズを取得し、サイズ変更を検知
    HWND hwnd = engine_->GetWindow()->GetHWND();
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    int32_t currentWidth = clientRect.right - clientRect.left;
    int32_t currentHeight = clientRect.bottom - clientRect.top;

    static int32_t lastWidth = currentWidth;
    static int32_t lastHeight = currentHeight;

    if (currentWidth > 0 && currentHeight > 0 && 
        (currentWidth != lastWidth || currentHeight != lastHeight)) {
        
        // 1. スワップチェーンと深度バッファ（DSV）のリサイズ
        engine_->GetDxCommon()->ResizeSwapChain(currentWidth, currentHeight);

        // 2. ポストプロセス（RTV）も同期リサイズして、D3D12の「RTVとDSVの寸法一致制約」を充足
        postProcess_->Resize(currentWidth, currentHeight);

        // 3. カメラのプロジェクションアスペクト比はゲーム基準の 16:9 に固定（歪み防止）
        constexpr float kGameAspectWidth = 16.0f;
        constexpr float kGameAspectHeight = 9.0f;
        constexpr float kGameAspectRatio = kGameAspectWidth / kGameAspectHeight;
        cameraMgr_->UpdateAllProjection(kGameAspectRatio);

        // 4. "Zoom" カメラ（右画面70%表示）が存在する場合、分割比率（0.7）を反映したアスペクト比を再設定
        constexpr float kSplitRightRatio = 0.7f;
        if (auto* zoomCam = cameraMgr_->GetCamera(CameraManager::kZoomCameraName)) {
            zoomCam->UpdateProjection(kGameAspectRatio * kSplitRightRatio);
        }

        // 5. 2Dプロジェクション行列もリサイズ後の実解像度に同期
        cameraMgr_->SetProjectionMatrix2D(Math::MakeOrthographicMatrix(
            0.0f, 0.0f,
            static_cast<float>(currentWidth),
            static_cast<float>(currentHeight),
            0.0f, 100.0f
        ));

        lastWidth = currentWidth;
        lastHeight = currentHeight;
    }

    // FPSおよび物理メモリの計測
    static auto lastTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    if (deltaTime < 0.0001f) { deltaTime = 0.0001f; }
    if (deltaTime > 1.0f) { deltaTime = 1.0f; }
    float currentFps = 1.0f / deltaTime;

    static float cachedMem = 0.0f;
    static float memTimer = 0.0f;
    memTimer -= deltaTime;
    if (memTimer <= 0.0f || cachedMem == 0.0f) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            cachedMem = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
        }
        memTimer = 0.5f; // 0.5秒ごとに更新
    }
    float currentMem = cachedMem;

    // 高負荷（スパイク）警告ログの出力処理（マジックナンバー排除）
    static float spikeWarningCooldown = 0.0f;
    if (spikeWarningCooldown > 0.0f) {
        spikeWarningCooldown -= deltaTime;
    }
    constexpr float kSpikeFpsThreshold = 30.0f;
    constexpr float kSpikeWarningCooldownMax = 5.0f; // クールタイムは5秒間

#ifdef _USEIMGUI
    if (currentFps < kSpikeFpsThreshold && spikeWarningCooldown <= 0.0f) {
        Log::Write(std::format("[警告] ★高負荷スパイク検知: FPSが一時的に低下しました ({:.1f} FPS) | 物理メモリ使用量: {:.2f} MB | フレーム時間: {:.4f} 秒", currentFps, currentMem, deltaTime));
        spikeWarningCooldown = kSpikeWarningCooldownMax;
    }
#endif

    bool isPaused = false;
    bool isGameViewVisible = false;

    // --- ImGui ---
#ifdef _USEIMGUI
    engine_->ImGuiBegin();
    
    // ゲーム側（シーン固有）のデバッグUIを描画
    SceneManager::GetInstance()->ImGuiControl();
    
    engine_->ImGuiEnd();
    if (auto debugEditor = engine_->GetDebugEditor()) {
        isGameViewVisible = debugEditor->IsGameViewVisible();
        isPaused = debugEditor->IsPaused();
    }
#endif

    // --- カメラ制御（ユーザー設計：ゲーム画面優先 ＆ シーン画面時のみ俯瞰 ＆ 復帰時は直前ゲームカメラ） ---
    static std::string sCurrentGameCamera = "";
    enum class EditorTab {
        GameView,
        SceneView
    };
    static EditorTab sCurrentActiveTab = EditorTab::GameView;

    // 現在アクティブなカメラが Editor 以外なら、常にゲーム側が意図した正規カメラとして追従・記録
    std::string activeCamName = cameraMgr_->GetActiveCameraName();
    if (!activeCamName.empty() && activeCamName != CameraManager::kEditorCameraName) {
        sCurrentGameCamera = activeCamName;
    }
    if (sCurrentGameCamera.empty()) {
        sCurrentGameCamera = cameraMgr_->GetDefaultGameCameraName();
    }

#ifdef _USEIMGUI
    // 表示中のタブ判定（重なっているタブは前面のみ Visible が true になる）
    bool isGameViewVisibleNow = GameViewWindow::IsGameViewVisible();
    bool isSceneViewVisibleNow = SceneViewWindow::IsSceneViewVisible();

    EditorTab determinedTab = sCurrentActiveTab;
    if (isGameViewVisibleNow && !isSceneViewVisibleNow) {
        determinedTab = EditorTab::GameView;
    } else if (isSceneViewVisibleNow && !isGameViewVisibleNow) {
        determinedTab = EditorTab::SceneView;
    } else if (isGameViewVisibleNow && isSceneViewVisibleNow) {
        if (GameViewWindow::IsGameViewFocused() || GameViewWindow::IsMouseOnGameView()) {
            determinedTab = EditorTab::GameView;
        } else if (SceneViewWindow::IsSceneViewFocused() || SceneViewWindow::IsMouseOnSceneView()) {
            determinedTab = EditorTab::SceneView;
        }
    }

    // タブ遷移の処理
    if (determinedTab == EditorTab::SceneView && sCurrentActiveTab != EditorTab::SceneView) {
        // 【シーン画面になった時】: 俯瞰にする
        sCurrentActiveTab = EditorTab::SceneView;
        if (cameraMgr_->HasCamera(CameraManager::kEditorCameraName)) {
            BaseCamera* gameCam = cameraMgr_->GetCamera(sCurrentGameCamera);
            if (auto* dc = dynamic_cast<DebugCamera*>(cameraMgr_->GetCamera(CameraManager::kEditorCameraName))) {
                if (gameCam) {
                    dc->SetPosition(gameCam->GetPosition());
                    dc->SetRotation(gameCam->GetCalculatedRotation());
                }
                dc->SetActive(true);
            }
            if (SceneViewWindow::GetCameraMode() == SceneViewWindow::CameraMode::DebugCamera) {
                cameraMgr_->SetActiveCamera(CameraManager::kEditorCameraName);
            }
        }
    } else if (determinedTab == EditorTab::GameView && sCurrentActiveTab != EditorTab::GameView) {
        // 【ゲーム画面に戻る時】: もともと使っていたゲームカメラに戻す
        sCurrentActiveTab = EditorTab::GameView;
        if (!sCurrentGameCamera.empty() && cameraMgr_->HasCamera(sCurrentGameCamera)) {
            cameraMgr_->SetActiveCamera(sCurrentGameCamera);
        }
    }

    // ゲーム画面表示中に Editor（俯瞰）が残っている場合は、もともと使っていたゲームカメラに復帰
    if (determinedTab == EditorTab::GameView) {
        if (cameraMgr_->GetActiveCameraName() == CameraManager::kEditorCameraName) {
            if (!sCurrentGameCamera.empty() && cameraMgr_->HasCamera(sCurrentGameCamera)) {
                cameraMgr_->SetActiveCamera(sCurrentGameCamera);
            }
        }
    }
#else
    EditorTab determinedTab = EditorTab::GameView;
#endif


    // --- 更新 ---

    input_->Update();
    
    // シーン遷移予約がある場合は、即座にシーン切り替えを実行する
    bool sceneChanged = SceneManager::GetInstance()->ProcessPendingSceneChange();
    if (sceneChanged) {
        // 1. 新シーンが自身で設定したアクティブカメラ（"Zoom" など）があれば最優先で採用
        std::string currentActive = cameraMgr_->GetActiveCameraName();
        if (!currentActive.empty() && currentActive != CameraManager::kEditorCameraName) {
            sCurrentGameCamera = currentActive;
        } else {
            sCurrentGameCamera = cameraMgr_->GetDefaultGameCameraName();
        }

        // 2. デバッグカメラの同期（新シーンのゲームカメラ位置・回転に合わせる）
        if (!sCurrentGameCamera.empty() && cameraMgr_->HasCamera(sCurrentGameCamera) && cameraMgr_->HasCamera(CameraManager::kEditorCameraName)) {
            BaseCamera* gameCam = cameraMgr_->GetCamera(sCurrentGameCamera);
            if (auto* dc = dynamic_cast<DebugCamera*>(cameraMgr_->GetCamera(CameraManager::kEditorCameraName))) {
                if (gameCam) {
                    dc->SetPosition(gameCam->GetPosition());
                    dc->SetRotation(gameCam->GetCalculatedRotation());
                }
            }
        }

        // 3. 現在開いているタブに応じてアクティブカメラを設定
        if (determinedTab == EditorTab::SceneView) {
            cameraMgr_->SetActiveCamera(CameraManager::kEditorCameraName);
        } else {
            cameraMgr_->SetActiveCamera(sCurrentGameCamera);
        }
    }

    if (!isPaused) {
        lightMgr_->Update();
        Log::Update(deltaTime);
        SceneManager::GetInstance()->Update();

        // ゲーム実行中であっても、アクティブカメラが DebugCamera であればマウス・キー操作で飛び回れるように更新！
        BaseCamera* activeCam = cameraMgr_->GetActiveCamera();
        if (auto* dc = dynamic_cast<DebugCamera*>(activeCam)) {
            dc->Update(input_.get());
        }

        cameraMgr_->Update();
    } else {
        // ポーズ中の更新処理：
        // Game View が表示されている場合のみ、オブジェクト編集やカメラ見回しを反映させる
        if (isGameViewVisible) {
            // ルート描画・敵配置モード（"Zoom" カメラ使用時）の場合は、ポーズ中もフェーズの更新（カメラ移動や敵配置、アスペクト比同期）を実行
            if (cameraMgr_->GetActiveCameraName() == CameraManager::kZoomCameraName || sCurrentGameCamera == CameraManager::kZoomCameraName) {
                SceneManager::GetInstance()->Update();
            }

            cameraMgr_->Update();
            lightMgr_->Update();

            // もしアクティブカメラが DebugCamera であれば、ポーズ中も入力操作で飛び回れるように更新する
            BaseCamera* activeCam = cameraMgr_->GetActiveCamera();
            if (auto* dc = dynamic_cast<DebugCamera*>(activeCam)) {
                dc->Update(input_.get());
            }

            // シーン内のすべてのオブジェクトを更新して行列再計算（位置・色変更）を即座に反映させる（アニメーションは停止）
            const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
            for (auto* obj : objects) {
                if (obj) {
                    obj->UpdateEditor();
                }
            }

            // エフェクトの行列（WVP）もポーズ中に再計算して、デバッグカメラの移動を反映させる
            EffectManager::GetInstance()->UpdateMatrices();
        }
    }


    // --- 描画 ---
    engine_->BeginFrame();

    // ポーズ中かつ Game View が非表示の場合は、本編 3D レンダリングを完全にスキップして負荷を最小にする
    bool shouldDraw = !(isPaused && !isGameViewVisible);

    if (shouldDraw) {
        // 1. 3Dシーン描画パス（RenderTextureへの描画）
        postProcess_->PreDraw();

        SceneManager::GetInstance()->Draw();

        postProcess_->PostDraw();

        // 2. ポストエフェクト処理（3D結果に対して深度輪郭線等のエフェクトを適用し、最終テクスチャを確定）
        postProcess_->ProcessEffects();

        // 3. 2D / UI描画パス（ポストエフェクト適用後の最終テクスチャに対するオーバーレイ描画）
        postProcess_->PreDraw2D();

        SceneManager::GetInstance()->Draw2D();

        postProcess_->PostDraw2D();

        // 4. Game Viewが表示されていない場合、通常どおりスワップチェーン全体への描画コピーを行う
        if (!isGameViewVisible) {
            postProcess_->CopyToSwapChain();
        }
    }

    engine_->EndFrame();
}

void App::Finalize() {
    Log::Write(L"========================================= [アプリケーション終了処理開始] =========================================");

    // 1. シーンの破棄（シーン内のすべてのGameObject、コンポーネント、リソース参照を破棄）
    SceneManager::GetInstance()->ClearCurrentScene();

    // 2. コマンド履歴（Undo / Redo スタックに退避された GameObject 等）の明示的破棄
    //    （削除コマンドが GameObject を保持しているため、リソース解放漏れを防ぐためにクリア）
    CommandHistory::GetInstance()->Clear();

    // 3. シーン階層（ヒエラルキー）の選択・登録情報クリア
    SceneHierarchy::GetInstance()->Clear();

    // 4. エフェクトシステムの破棄
    EffectManager::GetInstance()->Finalize();

    // 5. GPUパイプラインの完了を同期待機（破棄リソースを参照する描画コマンドの完了を保証）
    if (engine_ && engine_->GetDxCommon()) {
        engine_->GetDxCommon()->FlushGPU();
    }

    // 6. アプリケーションが所有するグラフィックスマネージャ・リソースの明示的破棄
    //    （DxCommon / Device / CoUninitialize の前にすべて安全に解放する）
    postProcess_.reset();
    modelMgr_.reset();
    ModelResource::SetModelManager(nullptr);
    texMgr_.reset();
    TextureResource::SetTextureManager(nullptr);
    lightMgr_.reset();
    LightResource::SetLightManager(nullptr);
    cameraMgr_.reset();
    CameraResource::SetCameraManager(nullptr);
    input_.reset();
    InputResource::SetInput(nullptr);
    sceneFactory_.reset();

    // 7. 遅延解放キューに残ったリソースを完全解放
    DeferredReleaseManager::GetInstance()->ReleaseAll();

    // 8. エンジン基盤の終了処理（PSO、ImGui、DxCommon、Device破棄、CoUninitialize）
    engine_->Finalize();

    Log::Write(L"========================================= [アプリケーション終了処理完了] =========================================");
}

bool App::IsEnd() const {
    return !engine_->ProcessMessage();
}
