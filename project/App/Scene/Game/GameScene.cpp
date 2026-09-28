#include "App/Scene/Game/GameScene.h"
#include "App/Scene/Game/Phase/DrawRoutePhase.h"
#include "App/Scene/Game/Phase/PlayPhase.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Math/Collision/Collision.h"
#include <algorithm>
#include <fstream>
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectFactory.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "externals/imgui/imgui.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Debug/SceneHierarchy.h"
#include <cstdlib>
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "App/Scene/Game/Stage/Stage.h"

// 不要になったヒットエフェクト名定数を削除

/**
 * @brief ゲーム本編シーンの初期化処理
 * カメラ、ライト、Player、Enemyオブジェクトの生成と初期パラメータ設定を行います。
 */
void GameScene::Initialize() {
    // 0. ポストプロセスのポインタを取得してメンバ変数に保持
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    if (postProcess_) {
        //postProcess_->SetUnderwaterActive(true);
    }

    // 1. 各マネージャへのポインタをリソース管理者から取得
    cameraMgr_ = CameraResource::GetCameraManager();
    lightMgr_ = LightResource::GetLightManager();
    input_ = InputResource::GetInput();

    // 2. カメラの生成・初期化
    playCamera_ = std::make_unique<PlayCamera>();
    playCamera_->Initialize(cameraMgr_);
    playCamera_->GetCamera()->SetPosition(kTopDownCameraPos);
    playCamera_->GetCamera()->SetRotation(kTopDownCameraRot);

    // 3. デバッグカメラの生成とマネージャへの登録
    debugCamera_ = std::make_shared<DebugCamera>();
    debugCamera_->Initialize();
    cameraMgr_->AddCamera(kDebugCameraName, debugCamera_);

    // アプリ起動直後はメインカメラをアクティブ状態に設定する
    cameraMgr_->SetActiveCamera(kMainCameraName);

    // 4. ライト管理クラスの生成、初期化
    light_ = std::make_unique<GameLight>();
    light_->Initialize(lightMgr_);

    // 5. エフェクトシステムの初期化と全エフェクト登録
    auto effectMgr = EffectManager::GetInstance();
    effectMgr->Initialize();
    EffectFactory::GetInstance()->RegisterAllEffects();

    // 雨のエフェクト再生開始（ループ）
    EffectPlayParam rainParam;
    rainParam.isLoop = true;
    rainParam.position = { 0.0f, 0.0f, 0.0f };
    rainParam.scale = { 1.0f, 1.0f, 1.0f };
    effectMgr->PlayEffect3D(kRainEffectName, rainParam);

    // 6. プレイヤーおよび敵オブジェクトの生成と初期化
    player_ = std::make_unique<Player>();
    player_->Initialize();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(input_, playCamera_.get());

    reticle_ = std::make_unique<Reticle>();
    reticle_->Initialize();

    // ルートとステージエディタの生成と初期化
    route_ = std::make_unique<Route>();
    route_->Initialize(input_, cameraMgr_);

    // ズームカメラの生成と初期化（ルート初期化後に配置）
    float startZ = route_->GetCurrentAreaStartZ();
    drawRouteCamera_ = std::make_unique<DrawRouteCamera>();
    drawRouteCamera_->Initialize(cameraMgr_, startZ);

    stageEditor_ = std::make_unique<StageEditor>();
    stageEditor_->Initialize(&enemyManager_->GetEnemies());

    // 保存されたステージがあればロードする
    stageEditor_->LoadStage("resources/stages/stage1.json");

    // 7. マップステージの初期化
    stage_ = std::make_unique<Stage>();
    stage_->Initialize();

    // 2Dミニマップの生成と初期化
    minimap_ = std::make_unique<Minimap>();
    minimap_->Initialize(stage_.get());

    // 右画面用 3D 赤丸インジケータ (ズーム3D空間用)
    cursorIndicatorZoom_ = std::make_unique<SphereObject>();
    cursorIndicatorZoom_->Initialize();
    cursorIndicatorZoom_->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
    cursorIndicatorZoom_->SetScale({ 1.2f, 1.2f, 1.2f });

    // 初期フェーズ（ルート描画フェーズ）の開始
    TransitionToDraw();
}

/**
 * @brief ImGuiによるデバッグ表示処理
 */
void GameScene::ImGuiControl() {
#ifdef _USEIMGUI
    if (currentPhase_) {
        currentPhase_->ImGuiControl();
    }

    // カメラ切り替え等のマネージャパラメータを表示
    cameraMgr_->ImGuiControl();

    // ポストプロセスのパラメータ調整用ImGuiコントロール
    if (postProcess_) {
        postProcess_->ImGuiControl();
    }

    // エフェクトのパラメータ調整用ImGuiコントロール
    EffectManager::GetInstance()->ImGuiControl("Effects");
#endif
}

/**
 * @brief 毎フレーム更新処理（キー入力によるカメラ切り替え・オブジェクトの座標・パラメータ更新）
 */
void GameScene::Update() {
#ifdef _USEIMGUI
    // TABキーによりメインカメラとデバッグカメラを切り替える
    static constexpr int kCameraToggleKey = DIK_TAB; // カメラ切り替え用キー定数
    if (input_->Trigger(kCameraToggleKey)) {
        bool isCurrentlyDebug = (cameraMgr_->GetActiveCamera() == debugCamera_.get());
        cameraMgr_->SetActiveCamera(isCurrentlyDebug ? kMainCameraName : kDebugCameraName);
    }
#endif

    // 現在アクティブなカメラを判定し、デバッグカメラ更新のみ手動で行う
    BaseCamera* activeCamera = cameraMgr_->GetActiveCamera();
    DebugCamera* debugCamPtr = dynamic_cast<DebugCamera*>(activeCamera);

    if (debugCamPtr) {
        debugCamPtr->SetActive(true);
        debugCamPtr->Update(input_);
    } else {
        debugCamera_->SetActive(false);
    }

    if (currentPhase_) {
        currentPhase_->Update();
    }
}

/**
 * @brief 毎フレーム描画処理（3Dオブジェクトのレンダリングコマンド発行）
 */
void GameScene::Draw() {
    if (currentPhase_) {
        currentPhase_->Draw();
    }
}

/**
 * @brief プレイモードのゲーム開始処理
 */
void GameScene::StartGame() {
    // 1. ルートの確定処理
    route_->FinalizeRoute();
    
    currentDistance_ = 0.0f;

    // 2. 自機(Player)の位置をルートの始点に設定
    Vector3 startPlayerPos = route_->GetPositionAtDistance(0.0f);
    player_->SetPosition(startPlayerPos);

    // 3. 動的湧きデータの初期設定 (エリア開始ごとに構築)
    if (isEnemyEnabled_) {
#ifdef _USEIMGUI
        if (stageEditor_) {
            stageEditor_->SetSelectedIndex(-1);
            stageEditor_->SetEditMode(StageEditor::EditMode::None);
        }
#endif
        enemyManager_->SetupSpawnTriggers(enemyManager_->GetEnemies(), startPlayerPos.z);
    }

    // 4. プレイ開始時にカメラ位置と回転をプレイ用の位置にリセット
    Vector3 startTangent = route_->GetTangentAtDistance(0.0f);
    playCamera_->Reset(startPlayerPos, startTangent);
    cameraMgr_->SetActiveCamera("Main");

    // 5. プレイフェーズへ遷移
    TransitionToPlay();
}

void GameScene::TransitionToPlay() {
    currentPhase_ = std::make_unique<PlayPhase>(
        input_,
        cameraMgr_,
        route_.get(),
        stage_.get(),
        player_.get(),
        enemyManager_.get(),
        playCamera_.get(),
        reticle_.get(),
        currentDistance_,
        [this]() {
            // エリアクリア時のコールバック
            int currentArea = route_->GetCurrentAreaIndex();
            player_->SetAutoMoving(false);
            route_->ClearForNewArea();
            route_->SetupArea(currentArea + 1);
            currentDistance_ = 0.0f;
            drawRouteCamera_->SetTargetZoom(Vector3{ 0.0f, 0.0f, route_->GetCurrentAreaStartZ() });
            
            // 旧エリアの敵エンティティや湧き判定フラグを完全にリセット
            enemyManager_->Reset();

            TransitionToDraw();
        }
    );
    currentPhase_->Initialize();
}

void GameScene::TransitionToDraw() {
    auto drawPhase = std::make_unique<DrawRoutePhase>(
        input_,
        cameraMgr_,
        route_.get(),
        stage_.get(),
        minimap_.get(),
        drawRouteCamera_.get(),
        cursorIndicatorZoom_.get(),
        stageEditor_.get(),
        [this]() { StartGame(); }
    );
    drawPhase->SetShowRouteEditorPtr(&showRouteEditor_);
    currentPhase_ = std::move(drawPhase);
    currentPhase_->Initialize();
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;
