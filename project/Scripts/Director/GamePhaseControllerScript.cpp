#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Route/RouteManagerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Enemy/EnemySpawnManagerScript.h"
#include "Scripts/Enemy/EnemySpawnPointScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/2d/Text/TextObject2D.h"
#include "Engine/Graphics/Text/TextTextureGenerator.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/Scene.h"
#include "App/Scene/Core/SceneDirector.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // DirectX 12 2D 描画用カラー (RGBA: 0.0f - 1.0f)
    constexpr Vector4 kDxPanelBgColor = { 14.0f / 255.0f, 18.0f / 255.0f, 28.0f / 255.0f, 230.0f / 255.0f };
    constexpr Vector4 kDxPanelBorderColor = { 35.0f / 255.0f, 170.0f / 255.0f, 245.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kDxPanelConnectedBorderColor = { 40.0f / 255.0f, 230.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxPanelInnerBgColor = { 8.0f / 255.0f, 12.0f / 255.0f, 18.0f / 255.0f, 200.0f / 255.0f };
    constexpr Vector4 kDxPanelInnerBorderColor = { 25.0f / 255.0f, 90.0f / 255.0f, 140.0f / 255.0f, 150.0f / 255.0f };
    constexpr Vector4 kDxGridLineColor = { 40.0f / 255.0f, 70.0f / 255.0f, 95.0f / 255.0f, 120.0f / 255.0f };
    constexpr Vector4 kDxStartColor = { 40.0f / 255.0f, 240.0f / 255.0f, 100.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxGoalColor = { 255.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxGoalConnectedColor = { 40.0f / 255.0f, 240.0f / 255.0f, 140.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxDrawingRouteColor = { 255.0f / 255.0f, 180.0f / 255.0f, 30.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxCompletedRouteColor = { 20.0f / 255.0f, 220.0f / 255.0f, 255.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxPlayerIconColor = { 255.0f / 255.0f, 230.0f / 255.0f, 40.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxTextColor = { 220.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxSubTextColor = { 140.0f / 255.0f, 170.0f / 255.0f, 195.0f / 255.0f, 1.0f };

    // ボタン用カラー
    constexpr Vector4 kDxMapResetBtnNormalBg = { 45.0f / 255.0f, 55.0f / 255.0f, 75.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kDxMapResetBtnHoverBg = { 75.0f / 255.0f, 95.0f / 255.0f, 130.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxGoBtnNormalBg = { 15.0f / 255.0f, 150.0f / 255.0f, 75.0f / 255.0f, 230.0f / 255.0f };
    constexpr Vector4 kDxGoBtnHoverBg = { 35.0f / 255.0f, 210.0f / 255.0f, 105.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxResetBtnNormalBg = { 40.0f / 255.0f, 50.0f / 255.0f, 65.0f / 255.0f, 200.0f / 255.0f };
    constexpr Vector4 kDxResetBtnHoverBg = { 65.0f / 255.0f, 80.0f / 255.0f, 105.0f / 255.0f, 240.0f / 255.0f };

    // 走行戦闘HUD用カラー
    constexpr Vector4 kDxHudHpBarBg = { 10.0f / 255.0f, 15.0f / 255.0f, 20.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kDxHpGreen = { 40.0f / 255.0f, 230.0f / 255.0f, 110.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxHpYellow = { 240.0f / 255.0f, 200.0f / 255.0f, 40.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxHpRed = { 240.0f / 255.0f, 50.0f / 255.0f, 50.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxAmmoAlert = { 255.0f / 255.0f, 140.0f / 255.0f, 30.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxReloadingColor = { 255.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 240.0f / 255.0f };

    // パルス演出定数
    constexpr float kGoalPulseSpeed = 2.5f;
    constexpr float kGoalPulseSpread = 10.0f;
    constexpr float kGoalPulseMaxAlpha = 220.0f;

    // レイアウト定数
    constexpr float kHeaderHeight = 36.0f;
    constexpr float kRouteLineThickness = 2.5f;
    constexpr float kCompletedLineThickness = 3.0f;
    constexpr float kMapBoundaryX = 50.0f; // ルート境界半幅

    // 自機初期配置定数
    constexpr float kDefaultPlayerPosX = 0.0f;
    constexpr float kDefaultPlayerPosY = 0.0f;
    constexpr Vector3 kDefaultPlayerForward = { 0.0f, 0.0f, 1.0f };

    // ガイダンス・カウントダウン用カラー
    constexpr Vector4 kDxGuidanceBgColor = { 10.0f / 255.0f, 15.0f / 255.0f, 25.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kDxGuidanceBorderColor = { 40.0f / 255.0f, 180.0f / 255.0f, 255.0f / 255.0f, 230.0f / 255.0f };
    constexpr Vector4 kDxCountdownBgColor = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 230.0f / 255.0f };
    constexpr Vector4 kDxCountdownBorderColor = { 255.0f / 255.0f, 200.0f / 255.0f, 40.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxCountdownNumberColor = { 255.0f / 255.0f, 220.0f / 255.0f, 50.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxStartTextColor = { 40.0f / 255.0f, 240.0f / 255.0f, 120.0f / 255.0f, 1.0f };

    // ガイダンス・カウントダウン用テキスト定数
    const std::string kGuidanceDrawPrompt = "画面をドラッグしてスタートからゴールまで線を引いてください";
    const std::string kGuidanceGoalPrompt = "ルート完成！ [SPACE]キー または [GO!] ボタンで発進します";
    const std::string kCountdownSubText = "GET READY...";
    const std::string kCountdownStartText = "START!!";
    const std::string kCountdownStartSubText = "GO!! GO!! GO!!";
}

REGISTER_SCRIPT(GamePhaseControllerScript)

GamePhaseControllerScript::GamePhaseControllerScript()
    : ScriptComponent("GamePhaseControllerScript") {
}

GamePhaseControllerScript::~GamePhaseControllerScript() {
    // 2D DirectX12描画オブジェクト破棄時のクラッシュを防ぐためGPU処理を同期
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void GamePhaseControllerScript::Awake() {
    currentPhase_ = Phase::DrawRoute;
    currentArea_ = 0;
    currentDistance_ = 0.0f;
    isSceneChanging_ = false;
}

void GamePhaseControllerScript::Start() {
    // コンポーネント参照の解決
    if (owner_) {
        routeMgr_ = owner_->GetComponent<RouteManagerScript>();
        spawnMgr_ = owner_->GetComponent<EnemySpawnManagerScript>();
    }

    auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (scene && scene->GetDirector()) {
        if (!routeMgr_) routeMgr_ = scene->GetDirector()->GetScript<RouteManagerScript>();
        if (!spawnMgr_) spawnMgr_ = scene->GetDirector()->GetScript<EnemySpawnManagerScript>();
    }
    if (scene) {
        for (const auto& obj : scene->GetGameObjects()) {
            if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                playerScript_ = obj->GetComponent<PlayerControllerScript>();
                break;
            }
        }
    }

    // 効果音 (SE) のロード
    if (!isSeLoaded_) {
        if (auto* audio = Audio::GetInstance()) {
            seCountdownBeep_ = audio->LoadSound(kSeCountdownBeepPath);
            seStartSignal_ = audio->LoadSound(kSeStartSignalPath);
            seEngineBoost_ = audio->LoadSound(kSeEngineBoostPath);
            isSeLoaded_ = true;
        }
    }

    // DirectX 12 2D 描画オブジェクト群の初期化
    Init2DObjects();

    TransitionToDraw();
}

void GamePhaseControllerScript::OnUpdate() {
    // 依存スクリプトが未取得なら再検索
    if (!routeMgr_ && owner_) {
        routeMgr_ = owner_->GetComponent<RouteManagerScript>();
    }
    if (!spawnMgr_ && owner_) {
        spawnMgr_ = owner_->GetComponent<EnemySpawnManagerScript>();
    }
    if (!routeMgr_ || !spawnMgr_) {
        auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene && scene->GetDirector()) {
            if (!routeMgr_) routeMgr_ = scene->GetDirector()->GetScript<RouteManagerScript>();
            if (!spawnMgr_) spawnMgr_ = scene->GetDirector()->GetScript<EnemySpawnManagerScript>();
        }
    }
    if (!playerScript_) {
        auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    playerScript_ = obj->GetComponent<PlayerControllerScript>();
                    break;
                }
            }
        }
    }

    // シーン遷移中はゲームプレイ更新を停止
    if (isSceneChanging_) {
        return;
    }

    // ゲームオーバー判定（自機死亡時：溶解演出の完了を待ってからシーン遷移）
    if (playerScript_ && playerScript_->IsDead()) {
        playerScript_->SetAutoMoving(false); // 走行を停止してその場で溶解

        if (playerScript_->IsDeathAnimationFinished()) {
            isSceneChanging_ = true;
            SceneManager::GetInstance()->ChangeScene(kGameOverSceneName);
            return;
        }
        // 溶解演出中は走行等の更新を停止し、自機の溶解アニメーションをじっくり見せる
        return;
    }

    // フェーズごとの処理
    if (currentPhase_ == Phase::DrawRoute) {
        UpdateDrawRoutePhase();
    } else if (currentPhase_ == Phase::Countdown) {
        UpdateCountdownPhase();
    } else if (currentPhase_ == Phase::Play) {
        UpdatePlayPhase();
    }
}

namespace {
    constexpr float kEpsilon = 0.0001f;
}

/// <summary>
/// ルート手書きフェーズ（DrawRoute）におけるプレイヤーの入力を監視・処理します。
/// 
/// 【座標逆変換と入力処理フロー】
/// 1. ゲームビューのウィンドウサイズと解像度からマウスカーソル座標をクライアント空間へスケーリング。
/// 2. マウスが画面左下の2Dミニマップ領域上にある場合:
///    - パネル内側の余白（innerMin/Max）から正規化座標 (normX, normZ) を算出。
///    - normXからワールドX座標（[-kMapBoundaryX, +kMapBoundaryX]）、normZからエリアZ区間（[startZ, goalZ]）へ逆変換。
///    - routeMgr_->UpdateDrawing() へ3Dワールド座標を渡してルートを記録。
/// 3. マウスが3D空間ビュー上にある場合:
///    - アクティブカメラからレイを生成し、地面平面（Y=0）との幾何交点（intersectPos）を計算してルートを記録。
/// 4. 各種UIボタン（ミニマップ内RESET、GOボタン、下部RESET、Rキー、右クリック、SPACEキー）の押下判定。
/// </summary>
void GamePhaseControllerScript::UpdateDrawRoutePhase() {
    auto* input = InputResource::GetInput();
    if (!input || !routeMgr_) return;

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) return;

    // マウスがゲーム画面（GameViewWindow）上にない場合、または画面外領域の場合はルート入力を完全無視
    if (!GameViewWindow::IsGameViewVisible() || !GameViewWindow::IsMouseOnGameView() ||
        mousePos.x < 0.0f || mousePos.x > viewSize.x || mousePos.y < 0.0f || mousePos.y > viewSize.y) {
        return;
    }

    // レンダーターゲット解像度（1280x720）へのマウス座標スケーリング
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    float scaleX = kRenderWidth / viewSize.x;
    float scaleY = kRenderHeight / viewSize.y;
    float targetMouseX = mousePos.x * scaleX;
    float targetMouseY = mousePos.y * scaleY;

    // 2D マップパネルの領域計算（レンダーターゲット基準）
    float panelMinX = kMapPanelMarginLeft;
    float panelMinY = kRenderHeight - kMapPanelMarginBottom - kMapPanelHeight;
    float panelMaxX = panelMinX + kMapPanelWidth;
    float panelMaxY = panelMinY + kMapPanelHeight;

    float innerMinX = panelMinX + kMapInnerPadding;
    float innerMaxX = panelMaxX - kMapInnerPadding;
    float innerMinY = panelMinY + kHeaderHeight;
    float innerMaxY = panelMaxY - kMapInnerPadding;

    // 2D マップ内ミニリセットボタンの領域計算
    float mapResetBtnMinX = panelMaxX - kMapResetBtnMarginRight - kMapResetBtnWidth;
    float mapResetBtnMinY = panelMinY + kMapResetBtnMarginTop;
    float mapResetBtnMaxX = mapResetBtnMinX + kMapResetBtnWidth;
    float mapResetBtnMaxY = mapResetBtnMinY + kMapResetBtnHeight;

    // GOサインボタンの領域計算
    float goBtnMinX = (kRenderWidth - kGoButtonWidth) * 0.5f;
    float goBtnMinY = kRenderHeight - kGoButtonMarginBottom - kGoButtonHeight;
    float goBtnMaxX = goBtnMinX + kGoButtonWidth;
    float goBtnMaxY = goBtnMinY + kGoButtonHeight;

    float resetBtnMinX = (kRenderWidth - kResetButtonWidth) * 0.5f;
    float resetBtnMinY = goBtnMinY - kResetButtonHeight - kResetBtnSpacing;
    float resetBtnMaxX = resetBtnMinX + kResetButtonWidth;
    float resetBtnMaxY = resetBtnMinY + kResetButtonHeight;

    bool isMouseOver2DMap = (targetMouseX >= innerMinX && targetMouseX <= innerMaxX &&
                             targetMouseY >= innerMinY && targetMouseY <= innerMaxY);

    bool isMouseOverMapResetBtn = (targetMouseX >= mapResetBtnMinX && targetMouseX <= mapResetBtnMaxX &&
                                   targetMouseY >= mapResetBtnMinY && targetMouseY <= mapResetBtnMaxY);

    bool isMouseOverGoBtn = (routeMgr_->HasReachedGoal() &&
                             targetMouseX >= goBtnMinX && targetMouseX <= goBtnMaxX &&
                             targetMouseY >= goBtnMinY && targetMouseY <= goBtnMaxY);

    bool isMouseOverResetBtn = (targetMouseX >= resetBtnMinX && targetMouseX <= resetBtnMaxX &&
                                targetMouseY >= resetBtnMinY && targetMouseY <= resetBtnMaxY);

    bool isTrigger = input->MouseTrigger(0);
    bool isPress = input->MousePress(0);
    bool isRightTrigger = input->MouseTrigger(1);

    // 【常時リセット機能】
    // 描画中・完了後を問わず、Rキー、右クリック、2Dマップ内RESETボタン、下部RESETボタンのいずれかで即座にリセット
    if (input->Trigger(DIK_R) || isRightTrigger ||
        (isTrigger && isMouseOverMapResetBtn) ||
        (routeMgr_->HasReachedGoal() && isTrigger && isMouseOverResetBtn)) {
        routeMgr_->ResetRoute();
        return;
    }

    // GOボタンまたはSPACEキーでカウントダウン開始（ゴール到達時のみ）
    if (routeMgr_->HasReachedGoal()) {
        if (input->Trigger(DIK_SPACE) || (isTrigger && isMouseOverGoBtn)) {
            TransitionToCountdown();
            return;
        }
    }

    // 2D マップパネル上のドラッグ入力（リセットボタン等の上ではないとき）
    if (isMouseOver2DMap && !isMouseOverMapResetBtn) {
        float innerW = innerMaxX - innerMinX;
        float innerH = innerMaxY - innerMinY;
        if (innerW > 0.0f && innerH > 0.0f) {
            float normX = (targetMouseX - innerMinX) / innerW;
            float normZ = (innerMaxY - targetMouseY) / innerH;

            float worldX = (normX - 0.5f) * (2.0f * kMapBoundaryX);
            float startZ = routeMgr_->GetCurrentAreaStartZ();
            float goalZ = routeMgr_->GetCurrentAreaGoalZ();
            float worldZ = startZ + normZ * (goalZ - startZ);

            Vector3 worldPos = { worldX, 0.0f, worldZ };
            routeMgr_->UpdateDrawing(worldPos, isTrigger, isPress);
        }
    }
    // 3D 空間ビュー上のドラッグ入力（2Dパネルやボタン外のとき）
    else if (!isMouseOverGoBtn && !isMouseOverResetBtn && !isMouseOverMapResetBtn) {
        auto* cameraMgr = CameraResource::GetCameraManager();
        if (cameraMgr) {
            BaseCamera* camera = cameraMgr->GetActiveCamera();
            if (camera) {
                Vector2 scaledMousePos = { targetMouseX, targetMouseY };

                Vector3 rayStart = camera->GetPosition();
                Vector3 rayDir = { 0.0f, -1.0f, 0.0f };
                camera->CreateRay(scaledMousePos, kRenderWidth, kRenderHeight, rayStart, rayDir);

                if (std::abs(rayDir.y) > kEpsilon) {
                    float t = -rayStart.y / rayDir.y;
                    if (t >= 0.0f) {
                        Vector3 intersectPos = {
                            rayStart.x + rayDir.x * t,
                            0.0f,
                            rayStart.z + rayDir.z * t
                        };
                        routeMgr_->UpdateDrawing(intersectPos, isTrigger, isPress);
                    }
                }
            }
        }
    }
}

/// <summary>
/// 走行・戦闘フェーズ（Play）中の自機等速前進とゴール到達判定を処理します。
/// 走行距離 currentDistance_ を進行させ、ルートマネージャから求めた3D座標および接線方向を自機へ反映します。
/// </summary>
void GamePhaseControllerScript::UpdatePlayPhase() {
    float moveSpeed = playerScript_ ? playerScript_->GetMoveSpeed() : playerMoveSpeed_;
    currentDistance_ += moveSpeed;

    // 自機位置と向きをルートから更新
    Vector3 playerPos = routeMgr_->GetPositionAtDistance(currentDistance_);
    Vector3 playerDir = routeMgr_->GetTangentAtDistance(currentDistance_);

    if (auto* playerGo = playerScript_->GetOwner()) {
        playerGo->SetPosition(playerPos);
    }
    playerScript_->SetDirection(playerDir);

    // エリアゴール到達判定
    if (currentDistance_ >= routeMgr_->GetTotalDistance()) {
        HandleAreaClear();
    }
}

/// <summary>
/// 現在のエリアのゴール到達時の処理を実行します。
/// 残留エネミー・弾丸を消去し、最終エリア（kMaxAreas走破）の場合はクリアシーンへ遷移、
/// 未完了の場合は次エリアをセットアップしてルート手書きフェーズへ移行します。
/// </summary>
void GamePhaseControllerScript::HandleAreaClear() {
    // すでにシーン遷移中なら多重実行を防止
    if (isSceneChanging_) {
        return;
    }

    // 前エリアの残敵・残留弾を一括消去
    ClearAllEnemies();

    // 最終エリア走破判定 (currentArea_ は 0-indexed なので 0, 1, ..., kMaxAreas - 1)
    if (currentArea_ + 1 >= kMaxAreas) {
        // 全エリアクリア
        isSceneChanging_ = true;
        currentArea_ = kMaxAreas - 1; // 表示上「AREA 4/4」に固定
        if (routeMgr_) {
            currentDistance_ = routeMgr_->GetTotalDistance(); // 100% に固定
        }
        if (playerScript_) {
            playerScript_->SetAutoMoving(false); // 自機の自動走行を即時停止
        }
        SceneManager::GetInstance()->ChangeScene(kClearSceneName);
        return;
    }

    // 次のエリアへ移行
    currentArea_++;
    if (routeMgr_) {
        routeMgr_->SetupArea(currentArea_);
    }
    if (spawnMgr_ && routeMgr_) {
        spawnMgr_->ResetSpawns(routeMgr_->GetCurrentAreaStartZ());
        spawnMgr_->SetCurrentArea(currentArea_);
    }
    currentDistance_ = 0.0f;
    TransitionToDraw();
}

/// <summary>
/// ルート手書きフェーズ（DrawRoute）への遷移設定を行います。
/// 前エリアの残存敵・弾丸を完全消去し、照準レティクルを非表示化、自機走行を停止し、
/// カメラを斜め立体俯瞰の見下ろし画角へ切り替えます。
/// </summary>
void GamePhaseControllerScript::TransitionToDraw() {
    currentPhase_ = Phase::DrawRoute;

    // 前エリアの敵・弾を完全消去し、照準レティクルを非表示化
    ClearAllEnemies();
    SetReticleVisible(false);

    // 自機（プレイヤー）の自動走行停止およびエリア開始地点への位置・向き初期化
    if (playerScript_) {
        playerScript_->SetAutoMoving(false);
        if (auto* playerGo = playerScript_->GetOwner()) {
            float startZ = routeMgr_ ? routeMgr_->GetCurrentAreaStartZ() : 0.0f;
            playerGo->SetPosition({ kDefaultPlayerPosX, kDefaultPlayerPosY, startZ });
        }
        playerScript_->SetDirection(kDefaultPlayerForward);
    }

    // カメラを見下ろし視点へ切り替え
    auto* cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && routeMgr_) {
        BaseCamera* camera = cameraMgr->GetActiveCamera();
        if (camera) {
            float startZ = routeMgr_->GetCurrentAreaStartZ();
            Vector3 camPos = { 0.0f, kDrawCameraHeight, startZ + kDrawCameraZOffset };
            camera->SetPosition(camPos);
            camera->SetRotation({ kDrawCameraPitch, 0.0f, 0.0f });
        }
    }
}

/// <summary>
/// 発進前カウントダウンフェーズ（Countdown）への遷移設定を行います。
/// ルートを確定し、自機をルート始点に配置・整列させ、発進カウントダウンを開始します。
/// </summary>
void GamePhaseControllerScript::TransitionToCountdown() {
    currentPhase_ = Phase::Countdown;
    countdownTimer_ = kCountdownDuration;
    lastCountdownSec_ = -1;
    pulseTimer_ = 0.0f;
    flashAlpha_ = 0.0f;
    lastRenderedCountdownText_.clear();
    lastRenderedCountdownSubText_.clear();

    // 照準レティクルは発進まで非表示
    SetReticleVisible(false);

    // ルート確定
    if (routeMgr_) {
        routeMgr_->FinalizeRoute();
    }

    // 自機をスタート位置に配置し進行方向に向ける
    if (playerScript_) {
        playerScript_->SetAutoMoving(false);
        if (routeMgr_) {
            Vector3 startPos = routeMgr_->GetPositionAtDistance(0.0f);
            Vector3 startDir = routeMgr_->GetTangentAtDistance(0.0f);
            if (auto* go = playerScript_->GetOwner()) {
                go->SetPosition(startPos);
            }
            playerScript_->SetDirection(startDir);
        }
    }
}

/// <summary>
/// 発進前カウントダウンフェーズの毎フレーム更新処理を行います。
/// タイマー減算、SE再生、エンジンエフェクト、白フラッシュ演出、完了時のTransitionToPlay遷移を制御します。
/// </summary>
void GamePhaseControllerScript::UpdateCountdownPhase() {
    constexpr float kDeltaTime = 1.0f / 60.0f;
    countdownTimer_ -= kDeltaTime;
    pulseTimer_ += kDeltaTime;

    // 発進白フラッシュの指数減衰
    if (flashAlpha_ > 0.0f) {
        flashAlpha_ -= kFlashFadeSpeed * kDeltaTime;
        if (flashAlpha_ < 0.0f) flashAlpha_ = 0.0f;
    }

    constexpr float kStartThreshold = 0.8f;
    if (countdownTimer_ > kStartThreshold) {
        int sec = static_cast<int>(std::ceil(countdownTimer_ - kStartThreshold));
        if (sec != lastCountdownSec_) {
            lastCountdownSec_ = sec;
            pulseTimer_ = 0.0f; // パルスアニメーションをリセット

            // 1. カウント音（ピッ）再生
            if (auto* audio = Audio::GetInstance()) {
                audio->PlaySoundW(seCountdownBeep_, 0.8f, false);
            }
        }
    } else {
        // START!! 突入時（1回だけ発火）
        if (lastCountdownSec_ != 0) {
            lastCountdownSec_ = 0;
            pulseTimer_ = 0.0f;
            flashAlpha_ = kFlashInitialAlpha; // 全画面白フラッシュ発動！

            // 1. スタートシグナル音（ポーン）再生
            if (auto* audio = Audio::GetInstance()) {
                audio->PlaySoundW(seStartSignal_, 0.95f, false);
            }
        }
    }

    if (countdownTimer_ <= 0.0f) {
        TransitionToPlay();
    }
}

/// <summary>
/// 走行・戦闘フェーズ（Play）への遷移設定を行います。
/// 発進エンジン音の再生、照準レティクルUI表示、敵スポーンタイマーの初期化、
/// 自機の自動走行フラグ有効化を実行します。
/// </summary>
void GamePhaseControllerScript::TransitionToPlay() {
    currentPhase_ = Phase::Play;
    currentDistance_ = 0.0f;

    // 発進エンジン音（ギュン！）再生
    if (auto* audio = Audio::GetInstance()) {
        audio->PlaySoundW(seEngineBoost_, 0.85f, false);
    }

    // 照準レティクルを表示
    SetReticleVisible(true);

    if (spawnMgr_ && routeMgr_) {
        spawnMgr_->ResetSpawns(routeMgr_->GetCurrentAreaStartZ());
    }

    if (routeMgr_) {
        routeMgr_->FinalizeRoute();
    }

    if (playerScript_) {
        playerScript_->SetAutoMoving(true);
        if (routeMgr_) {
            Vector3 startPos = routeMgr_->GetPositionAtDistance(0.0f);
            Vector3 startDir = routeMgr_->GetTangentAtDistance(0.0f);
            if (auto* go = playerScript_->GetOwner()) {
                go->SetPosition(startPos);
            }
            playerScript_->SetDirection(startDir);
        }
    }
}

void GamePhaseControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.8f, 0.2f, 1.0f };
    constexpr ImVec4 kSubHeaderColor = { 0.4f, 0.8f, 1.0f, 1.0f };
    constexpr float kResetBtnWidth = 140.0f;
    constexpr float kResetBtnHeight = 24.0f;

    ImGui::TextColored(kHeaderColor, "【ゲーム進行フェーズ管理者】");
    ImGui::Separator();

    ImGui::Text("現在エリア: エリア %d / %d", currentArea_ + 1, kMaxAreas);
    ImGui::Text("進行フェーズ: %s", currentPhase_ == Phase::DrawRoute ? "ルート描画 (DrawRoute)" : "走行戦闘 (Play)");
    ImGui::Text("現在走行距離: %.2f m", currentDistance_);
    ImGui::Spacing();

    // 自機スクリプトが存在する場合は速度を双方向同期
    if (playerScript_) {
        playerMoveSpeed_ = playerScript_->GetMoveSpeed();
    }
    if (ImGui::DragFloat("ルート走行速度##PlayerMoveSpeed", &playerMoveSpeed_, 0.005f, kMinPlayerMoveSpeed, kMaxPlayerMoveSpeed, "%.3f")) {
        if (playerScript_) {
            playerScript_->SetMoveSpeed(playerMoveSpeed_);
        }
    }
    if (ImGui::Button("速度リセット (0.15)##ResetSpeed", ImVec2(kResetBtnWidth, kResetBtnHeight))) {
        playerMoveSpeed_ = kDefaultPlayerMoveSpeed;
        if (playerScript_) {
            playerScript_->SetMoveSpeed(kDefaultPlayerMoveSpeed);
        }
    }
    ImGui::Separator();

    if (currentPhase_ == Phase::DrawRoute) {
        if (routeMgr_ && routeMgr_->HasReachedGoal()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "ゴール到達！GOボタンで走行開始できます。");
            if (ImGui::Button("走行開始 (Start Play)##StartPlayBtn")) {
                TransitionToPlay();
            }
        } else {
            ImGui::TextDisabled("画面上をドラッグしてスタートからゴールまで線を引いてください。");
        }
    } else {
        ImGui::Separator();
        ImGui::TextColored(kSubHeaderColor, "【HUD設定】");
        ImGui::Checkbox("上部方位コンパス表示##EnableCompass", &isCompassEnabled_);
        ImGui::Spacing();

        if (ImGui::Button("ルート描画に戻る##BackToDrawBtn")) {
            TransitionToDraw();
        }
    }
#endif
}

void GamePhaseControllerScript::Draw2D() {
    if (!is2DInitialized_) {
        Init2DObjects();
    }

    // 1. ルート手書きフェーズ中
    if (currentPhase_ == Phase::DrawRoute) {
        // 2Dミニマップの描画 (DirectX 12)
        Render2DMapDirectX();

        // 案内ガイダンスの描画 (DirectX 12)
        RenderGuidanceDirectX();

        // ゴール到達時のGOサイン・リセットボタンの描画 (DirectX 12)
        if (routeMgr_ && routeMgr_->HasReachedGoal()) {
            RenderGoSignDirectX();
        }
    } else if (currentPhase_ == Phase::Countdown) {
        // 2. 発進前カウントダウンフェーズ中の演出描画 (DirectX 12)
        RenderCountdownDirectX();
    } else if (currentPhase_ == Phase::Play) {
        // 3. 走行戦闘プレイフェーズ中のHUD描画 (DirectX 12)
        RenderPlayHUDDirectX();
    }

    // 4. 発進白フラッシュ描画（全フェーズ共通の最前面オーバーレイ）
    if (flashSprite_ && flashAlpha_ > 0.005f) {
        constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
        constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
        flashSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        flashSprite_->SetSize(kRenderWidth, kRenderHeight);
        flashSprite_->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, flashAlpha_ };
        flashSprite_->Update();
        flashSprite_->Draw("white", true);
    }
}

void GamePhaseControllerScript::OnGUI() {
    // Release構成・Debug構成共通で DirectX 12 描画パイプライン（Draw2D）に完全移行済み
}

/// <summary>
/// 【旧HUD描画メソッド】DirectX 12描画パイプライン（Render2DMapDirectX）へ移行済みのため空実装として保持。
/// </summary>
void GamePhaseControllerScript::DrawHUD() {
}

/// <summary>
/// 【旧2Dマップ描画メソッド】DirectX 12描画パイプライン（Render2DMapDirectX）へ移行済みのため空実装として保持。
/// </summary>
void GamePhaseControllerScript::Draw2DMap(const Vector2&, const Vector2&) {
}

/// <summary>
/// 【旧GOサイン描画メソッド】DirectX 12描画パイプライン（RenderGoSignDirectX）へ移行済みのため空実装として保持。
/// </summary>
void GamePhaseControllerScript::DrawGoSign(const Vector2&, const Vector2&) {
}

/// <summary>
/// 2Dミニマップパネル、枠線、グリッド線、スタート/ゴール円、自機アイコン、GOボタン等の
/// DirectX 12 2Dスプライト・ライン・テキストオブジェクトを一括生成して初期化します。
/// </summary>
void GamePhaseControllerScript::Init2DObjects() {
    if (is2DInitialized_) return;

    // 1. パネル外枠・内枠スプライト
    mapBgSprite_ = std::make_unique<SpriteObject>();
    mapBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(mapBgSprite_.get());
    mapBgSprite_->SetSize(kMapPanelWidth, kMapPanelHeight);
    mapBgSprite_->GetMaterialData()->color = kDxPanelBgColor;

    mapInnerSprite_ = std::make_unique<SpriteObject>();
    mapInnerSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(mapInnerSprite_.get());
    mapInnerSprite_->GetMaterialData()->color = kDxPanelInnerBgColor;

    // 2. 外枠4辺
    for (int i = 0; i < 4; ++i) {
        mapBorders_[i] = std::make_unique<Line2DObject>();
        mapBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(mapBorders_[i].get());
        mapBorders_[i]->SetThickness(2.0f);
        mapBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
    }

    // 3. 内枠4辺
    for (int i = 0; i < 4; ++i) {
        mapInnerBorders_[i] = std::make_unique<Line2DObject>();
        mapInnerBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(mapInnerBorders_[i].get());
        mapInnerBorders_[i]->SetThickness(1.0f);
        mapInnerBorders_[i]->GetMaterialData()->color = kDxPanelInnerBorderColor;
    }

    // 4. グリッド線 (中央線1本 + 横3本)
    constexpr int kGridLineCount = 4;
    gridLines_.reserve(kGridLineCount);
    for (int i = 0; i < kGridLineCount; ++i) {
        auto line = std::make_unique<Line2DObject>();
        line->Initialize();
        SceneHierarchy::GetInstance()->Unregister(line.get());
        line->SetThickness(1.0f);
        line->GetMaterialData()->color = kDxGridLineColor;
        gridLines_.push_back(std::move(line));
    }

    // 5. スタート・ゴール・波紋・自機円
    startCircle_ = std::make_unique<Circle2DObject>();
    startCircle_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(startCircle_.get());
    startCircle_->SetRadius(kCircleDrawRadius);
    startCircle_->GetMaterialData()->color = kDxStartColor;

    goalCircle_ = std::make_unique<Circle2DObject>();
    goalCircle_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(goalCircle_.get());
    goalCircle_->SetRadius(kCircleDrawRadius);
    goalCircle_->GetMaterialData()->color = kDxGoalColor;

    goalPulseRing_ = std::make_unique<Ring2DObject>();
    goalPulseRing_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(goalPulseRing_.get());
    goalPulseRing_->GetMaterialData()->color = kDxGoalConnectedColor;

    playerIconCircle_ = std::make_unique<Circle2DObject>();
    playerIconCircle_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(playerIconCircle_.get());
    playerIconCircle_->SetRadius(kPlayerIconRadius);
    playerIconCircle_->GetMaterialData()->color = kDxPlayerIconColor;

    // 6. ミニリセットボタン (2Dマップ内右上)
    mapResetBtnSprite_ = std::make_unique<SpriteObject>();
    mapResetBtnSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(mapResetBtnSprite_.get());
    mapResetBtnSprite_->SetSize(kMapResetBtnWidth, kMapResetBtnHeight);
    mapResetBtnSprite_->GetMaterialData()->color = kDxMapResetBtnNormalBg;

    mapResetBtnTextObj_ = std::make_unique<TextObject2D>();
    mapResetBtnTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(mapResetBtnTextObj_.get());
    mapResetBtnTextGen_ = std::make_unique<TextTextureGenerator>();
    mapResetBtnTextGen_->Generate("RESET", "Yu Gothic UI", 11.0f, 1, true, false, false);
    mapResetBtnTextObj_->SetSize(static_cast<float>(mapResetBtnTextGen_->GetWidth()), static_cast<float>(mapResetBtnTextGen_->GetHeight()));
    mapResetBtnTextObj_->SetColor(kDxTextColor);

    // 7. GOサインボタン & 下部リセットボタン
    goBtnSprite_ = std::make_unique<SpriteObject>();
    goBtnSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(goBtnSprite_.get());
    goBtnSprite_->SetSize(kGoButtonWidth, kGoButtonHeight);
    goBtnSprite_->GetMaterialData()->color = kDxGoBtnNormalBg;

    goBtnTextObj_ = std::make_unique<TextObject2D>();
    goBtnTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(goBtnTextObj_.get());
    goBtnTextGen_ = std::make_unique<TextTextureGenerator>();
    goBtnTextGen_->Generate("▶ LAUNCH // GO! ◀", "Yu Gothic UI", 18.0f, 1, true, false, false);
    goBtnTextObj_->SetSize(static_cast<float>(goBtnTextGen_->GetWidth()), static_cast<float>(goBtnTextGen_->GetHeight()));
    goBtnTextObj_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    resetBtnSprite_ = std::make_unique<SpriteObject>();
    resetBtnSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(resetBtnSprite_.get());
    resetBtnSprite_->SetSize(kResetButtonWidth, kResetButtonHeight);
    resetBtnSprite_->GetMaterialData()->color = kDxResetBtnNormalBg;

    resetBtnTextObj_ = std::make_unique<TextObject2D>();
    resetBtnTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(resetBtnTextObj_.get());
    resetBtnTextGen_ = std::make_unique<TextTextureGenerator>();
    resetBtnTextGen_->Generate("↺ RESET ROUTE", "Yu Gothic UI", 13.0f, 1, true, false, false);
    resetBtnTextObj_->SetSize(static_cast<float>(resetBtnTextGen_->GetWidth()), static_cast<float>(resetBtnTextGen_->GetHeight()));
    resetBtnTextObj_->SetColor(kDxTextColor);

    // 8. マップタイトルテキスト
    mapTitleTextObj_ = std::make_unique<TextObject2D>();
    mapTitleTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(mapTitleTextObj_.get());
    mapTitleTextGen_ = std::make_unique<TextTextureGenerator>();

    // 9. 走行戦闘中HUDパーツの初期化
    // (1) 自機HPパネル用パーツ
    hudHpBgSprite_ = std::make_unique<SpriteObject>();
    hudHpBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudHpBgSprite_.get());
    hudHpBgSprite_->SetSize(kHudHpPanelWidth, kHudHpPanelHeight);
    hudHpBgSprite_->GetMaterialData()->color = kDxPanelBgColor;

    hudHpBarBgSprite_ = std::make_unique<SpriteObject>();
    hudHpBarBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudHpBarBgSprite_.get());
    hudHpBarBgSprite_->SetSize(kHudHpPanelWidth - kHudHpBarPadding * 2.0f, kHudHpBarHeight);
    hudHpBarBgSprite_->GetMaterialData()->color = kDxHudHpBarBg;

    hudHpBarFillSprite_ = std::make_unique<SpriteObject>();
    hudHpBarFillSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudHpBarFillSprite_.get());
    hudHpBarFillSprite_->SetSize(kHudHpPanelWidth - kHudHpBarPadding * 2.0f, kHudHpBarHeight);
    hudHpBarFillSprite_->GetMaterialData()->color = kDxHpGreen;

    for (int i = 0; i < 4; ++i) {
        hudHpBorders_[i] = std::make_unique<Line2DObject>();
        hudHpBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hudHpBorders_[i].get());
        hudHpBorders_[i]->SetThickness(1.5f);
        hudHpBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
    }

    hudHpTextObj_ = std::make_unique<TextObject2D>();
    hudHpTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudHpTextObj_.get());
    hudHpTextGen_ = std::make_unique<TextTextureGenerator>();

    // (2) 残弾数パネル用パーツ
    hudAmmoBgSprite_ = std::make_unique<SpriteObject>();
    hudAmmoBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudAmmoBgSprite_.get());
    hudAmmoBgSprite_->SetSize(kHudAmmoPanelWidth, kHudAmmoPanelHeight);
    hudAmmoBgSprite_->GetMaterialData()->color = kDxPanelBgColor;

    for (int i = 0; i < 4; ++i) {
        hudAmmoBorders_[i] = std::make_unique<Line2DObject>();
        hudAmmoBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hudAmmoBorders_[i].get());
        hudAmmoBorders_[i]->SetThickness(1.5f);
        hudAmmoBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
    }

    hudAmmoTextObj_ = std::make_unique<TextObject2D>();
    hudAmmoTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudAmmoTextObj_.get());
    hudAmmoTextGen_ = std::make_unique<TextTextureGenerator>();

    // (3) エリア進行度パネル用パーツ
    hudAreaBgSprite_ = std::make_unique<SpriteObject>();
    hudAreaBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudAreaBgSprite_.get());
    hudAreaBgSprite_->SetSize(kHudAreaPanelWidth, kHudAreaPanelHeight);
    hudAreaBgSprite_->GetMaterialData()->color = kDxPanelBgColor;

    for (int i = 0; i < 4; ++i) {
        hudAreaBorders_[i] = std::make_unique<Line2DObject>();
        hudAreaBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hudAreaBorders_[i].get());
        hudAreaBorders_[i]->SetThickness(1.5f);
        hudAreaBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
    }

    hudAreaTextObj_ = std::make_unique<TextObject2D>();
    hudAreaTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudAreaTextObj_.get());
    hudAreaTextGen_ = std::make_unique<TextTextureGenerator>();

    // 10. ガイダンスパネル用パーツ (DirectX 12)
    guidanceBgSprite_ = std::make_unique<SpriteObject>();
    guidanceBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(guidanceBgSprite_.get());
    guidanceBgSprite_->SetSize(kGuidancePanelWidth, kGuidancePanelHeight);
    guidanceBgSprite_->GetMaterialData()->color = kDxGuidanceBgColor;

    for (int i = 0; i < 4; ++i) {
        guidanceBorders_[i] = std::make_unique<Line2DObject>();
        guidanceBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(guidanceBorders_[i].get());
        guidanceBorders_[i]->SetThickness(2.0f);
        guidanceBorders_[i]->GetMaterialData()->color = kDxGuidanceBorderColor;
    }

    guidanceTextObj_ = std::make_unique<TextObject2D>();
    guidanceTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(guidanceTextObj_.get());
    guidanceTextGen_ = std::make_unique<TextTextureGenerator>();
    lastGuidanceText_.clear();

    // 11. カウントダウン用パーツ (DirectX 12)
    countdownBgSprite_ = std::make_unique<SpriteObject>();
    countdownBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(countdownBgSprite_.get());
    countdownBgSprite_->SetSize(kCountdownBoxWidth, kCountdownBoxHeight);
    countdownBgSprite_->GetMaterialData()->color = kDxCountdownBgColor;

    for (int i = 0; i < 4; ++i) {
        countdownBorders_[i] = std::make_unique<Line2DObject>();
        countdownBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(countdownBorders_[i].get());
        countdownBorders_[i]->SetThickness(2.0f);
        countdownBorders_[i]->GetMaterialData()->color = kDxCountdownBorderColor;
    }

    countdownTextObj_ = std::make_unique<TextObject2D>();
    countdownTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(countdownTextObj_.get());
    countdownTextGen_ = std::make_unique<TextTextureGenerator>();

    countdownSubTextObj_ = std::make_unique<TextObject2D>();
    countdownSubTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(countdownSubTextObj_.get());
    countdownSubTextGen_ = std::make_unique<TextTextureGenerator>();

    // 12. 全画面白フラッシュ用スプライト (DirectX 12)
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    flashSprite_ = std::make_unique<SpriteObject>();
    flashSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(flashSprite_.get());
    flashSprite_->SetSize(kRenderWidth, kRenderHeight);
    flashSprite_->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, 0.0f };

    // 13. 画面上部コンパスHUD（方位バー）用パーツ (DirectX 12)
    compassBgSprite_ = std::make_unique<SpriteObject>();
    compassBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(compassBgSprite_.get());
    compassBgSprite_->SetSize(kCompassBarWidth, kCompassBarHeight);
    compassBgSprite_->GetMaterialData()->color = { 10.0f / 255.0f, 16.0f / 255.0f, 26.0f / 255.0f, 210.0f / 255.0f };

    for (int i = 0; i < 4; ++i) {
        compassBorders_[i] = std::make_unique<Line2DObject>();
        compassBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(compassBorders_[i].get());
        compassBorders_[i]->SetThickness(1.5f);
        compassBorders_[i]->GetMaterialData()->color = { 35.0f / 255.0f, 170.0f / 255.0f, 245.0f / 255.0f, 200.0f / 255.0f };
    }

    compassCenterMarker_ = std::make_unique<Line2DObject>();
    compassCenterMarker_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(compassCenterMarker_.get());
    compassCenterMarker_->SetThickness(2.5f);
    compassCenterMarker_->GetMaterialData()->color = { 40.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f, 1.0f };

    compassTickLines_.clear();
    compassTickLines_.reserve(kCompassTickPoolSize);
    for (int i = 0; i < kCompassTickPoolSize; ++i) {
        auto line = std::make_unique<Line2DObject>();
        line->Initialize();
        SceneHierarchy::GetInstance()->Unregister(line.get());
        line->SetThickness(1.0f);
        line->GetMaterialData()->color = { 140.0f / 255.0f, 180.0f / 255.0f, 210.0f / 255.0f, 180.0f / 255.0f };
        compassTickLines_.push_back(std::move(line));
    }

    struct LabelDef { float angle; const char* text; Vector4 color; };
    const LabelDef kLabelDefs[] = {
        { 0.0f,    "▲ FWD",  { 40.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f, 1.0f } },
        { 45.0f,   "R45",    { 200.0f / 255.0f, 220.0f / 255.0f, 240.0f / 255.0f, 0.9f } },
        { 90.0f,   "R90",    { 230.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f, 1.0f } },
        { 135.0f,  "R135",   { 200.0f / 255.0f, 220.0f / 255.0f, 240.0f / 255.0f, 0.9f } },
        { 180.0f,  "▼ BACK", { 255.0f / 255.0f, 140.0f / 255.0f, 40.0f / 255.0f, 1.0f } },
        { -135.0f, "L135",   { 200.0f / 255.0f, 220.0f / 255.0f, 240.0f / 255.0f, 0.9f } },
        { -90.0f,  "L90",    { 230.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f, 1.0f } },
        { -45.0f,  "L45",    { 200.0f / 255.0f, 220.0f / 255.0f, 240.0f / 255.0f, 0.9f } }
    };

    compassLabels_.clear();
    for (const auto& def : kLabelDefs) {
        CompassLabelItem item;
        item.angleDeg = def.angle;
        item.text = def.text;
        item.color = def.color;
        item.textObj = std::make_unique<TextObject2D>();
        item.textObj->Initialize();
        SceneHierarchy::GetInstance()->Unregister(item.textObj.get());
        item.textGen = std::make_unique<TextTextureGenerator>();
        if (item.textGen->Generate(def.text, "Yu Gothic UI", 11.0f, 1, true, false, false)) {
            item.textWidth = static_cast<float>(item.textGen->GetWidth());
            item.textHeight = static_cast<float>(item.textGen->GetHeight());
            item.textObj->SetSize(item.textWidth, item.textHeight);
        }
        compassLabels_.push_back(std::move(item));
    }

    is2DInitialized_ = true;
}

void GamePhaseControllerScript::Render2DMapDirectX() {
    if (!routeMgr_) return;

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    float targetMouseX = 0.0f;
    float targetMouseY = 0.0f;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        targetMouseX = mousePos.x * (kRenderWidth / viewSize.x);
        targetMouseY = mousePos.y * (kRenderHeight / viewSize.y);
    }

    // パネル領域の計算（レンダーターゲット1280x720の左下に配置）
    float panelMinX = kMapPanelMarginLeft;
    float panelMinY = kRenderHeight - kMapPanelMarginBottom - kMapPanelHeight;
    float panelMaxX = panelMinX + kMapPanelWidth;
    float panelMaxY = panelMinY + kMapPanelHeight;

    float innerMinX = panelMinX + kMapInnerPadding;
    float innerMaxX = panelMaxX - kMapInnerPadding;
    float innerMinY = panelMinY + kHeaderHeight;
    float innerMaxY = panelMaxY - kMapInnerPadding;
    float innerW = innerMaxX - innerMinX;
    float innerH = innerMaxY - innerMinY;

    bool hasReachedGoal = routeMgr_->HasReachedGoal();

    // 1. パネル背景描画
    mapBgSprite_->SetPosition({ panelMinX, panelMinY, 0.0f });
    mapBgSprite_->SetSize(kMapPanelWidth, kMapPanelHeight);
    mapBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
    mapBgSprite_->Update();
    mapBgSprite_->Draw("white", true);

    // 2. 内側マップ領域描画
    mapInnerSprite_->SetPosition({ innerMinX, innerMinY, 0.0f });
    mapInnerSprite_->SetSize(innerW, innerH);
    mapInnerSprite_->GetMaterialData()->color = kDxPanelInnerBgColor;
    mapInnerSprite_->Update();
    mapInnerSprite_->Draw("white", true);

    // 3. 外枠4辺描画
    Vector4 borderColor = hasReachedGoal ? kDxPanelConnectedBorderColor : kDxPanelBorderColor;
    float borderThickness = hasReachedGoal ? 2.0f : 1.5f;
    mapBorders_[0]->SetPoints({ panelMinX, panelMinY }, { panelMaxX, panelMinY });
    mapBorders_[1]->SetPoints({ panelMinX, panelMaxY }, { panelMaxX, panelMaxY });
    mapBorders_[2]->SetPoints({ panelMinX, panelMinY }, { panelMinX, panelMaxY });
    mapBorders_[3]->SetPoints({ panelMaxX, panelMinY }, { panelMaxX, panelMaxY });
    for (int i = 0; i < 4; ++i) {
        mapBorders_[i]->SetThickness(borderThickness);
        mapBorders_[i]->GetMaterialData()->color = borderColor;
        mapBorders_[i]->Update();
        mapBorders_[i]->Draw("white", true);
    }

    // 4. 内枠4辺描画
    mapInnerBorders_[0]->SetPoints({ innerMinX, innerMinY }, { innerMaxX, innerMinY });
    mapInnerBorders_[1]->SetPoints({ innerMinX, innerMaxY }, { innerMaxX, innerMaxY });
    mapInnerBorders_[2]->SetPoints({ innerMinX, innerMinY }, { innerMinX, innerMaxY });
    mapInnerBorders_[3]->SetPoints({ innerMaxX, innerMinY }, { innerMaxX, innerMaxY });
    for (int i = 0; i < 4; ++i) {
        mapInnerBorders_[i]->Update();
        mapInnerBorders_[i]->Draw("white", true);
    }

    // 5. グリッド線描画 (横3本、中央縦1本)
    constexpr float kGridRatiosH[3] = { 0.25f, 0.5f, 0.75f };
    for (int i = 0; i < 3 && i < static_cast<int>(gridLines_.size()); ++i) {
        float gy = innerMinY + innerH * kGridRatiosH[i];
        gridLines_[i]->SetPoints({ innerMinX, gy }, { innerMaxX, gy });
        gridLines_[i]->Update();
        gridLines_[i]->Draw("white", true);
    }
    if (gridLines_.size() >= 4) {
        float gx = innerMinX + innerW * 0.5f;
        gridLines_[3]->SetPoints({ gx, innerMinY }, { gx, innerMaxY });
        gridLines_[3]->Update();
        gridLines_[3]->Draw("white", true);
    }

    // 6. ヘッダータイトル描画
    char titleBuf[64];
    snprintf(titleBuf, sizeof(titleBuf), "AREA %d/%d - MAP", currentArea_ + 1, kMaxAreas);
    if (mapTitleTextGen_->Generate(titleBuf, "Yu Gothic UI", 12.0f, 0, true, false, false)) {
        mapTitleTextObj_->SetSize(static_cast<float>(mapTitleTextGen_->GetWidth()), static_cast<float>(mapTitleTextGen_->GetHeight()));
    }
    mapTitleTextObj_->SetPosition({ panelMinX + kMapInnerPadding, panelMinY + 10.0f, 0.0f });
    mapTitleTextObj_->SetColor(kDxTextColor);
    mapTitleTextObj_->Update();
    mapTitleTextObj_->Draw(mapTitleTextGen_->GetGpuHandle(), true);

    // 7. ミニリセットボタン描画
    float mapResetBtnMinX = panelMaxX - kMapResetBtnMarginRight - kMapResetBtnWidth;
    float mapResetBtnMinY = panelMinY + kMapResetBtnMarginTop;
    float mapResetBtnMaxX = mapResetBtnMinX + kMapResetBtnWidth;
    float mapResetBtnMaxY = mapResetBtnMinY + kMapResetBtnHeight;

    bool isHoverMapReset = (targetMouseX >= mapResetBtnMinX && targetMouseX <= mapResetBtnMaxX &&
                            targetMouseY >= mapResetBtnMinY && targetMouseY <= mapResetBtnMaxY);
    mapResetBtnSprite_->SetPosition({ mapResetBtnMinX, mapResetBtnMinY, 0.0f });
    mapResetBtnSprite_->SetSize(kMapResetBtnWidth, kMapResetBtnHeight);
    mapResetBtnSprite_->GetMaterialData()->color = isHoverMapReset ? kDxMapResetBtnHoverBg : kDxMapResetBtnNormalBg;
    mapResetBtnSprite_->Update();
    mapResetBtnSprite_->Draw("white", true);

    float btnTextX = mapResetBtnMinX + (kMapResetBtnWidth - mapResetBtnTextGen_->GetWidth()) * 0.5f;
    float btnTextY = mapResetBtnMinY + (kMapResetBtnHeight - mapResetBtnTextGen_->GetHeight()) * 0.5f;
    mapResetBtnTextObj_->SetPosition({ btnTextX, btnTextY, 0.0f });
    mapResetBtnTextObj_->Update();
    mapResetBtnTextObj_->Draw(mapResetBtnTextGen_->GetGpuHandle(), true);

    // 8. 座標変換ラムダ
    float startZ = routeMgr_->GetCurrentAreaStartZ();
    float goalZ = routeMgr_->GetCurrentAreaGoalZ();
    float areaLength = goalZ - startZ;
    constexpr float kDefaultAreaLength = 120.0f;
    if (areaLength <= 0.001f) areaLength = kDefaultAreaLength;

    auto WorldToMap2D = [&](const Vector3& worldPos) -> Vector2 {
        float normX = (worldPos.x + kMapBoundaryX) / (2.0f * kMapBoundaryX);
        float normZ = (worldPos.z - startZ) / areaLength;
        normX = std::clamp(normX, 0.0f, 1.0f);
        normZ = std::clamp(normZ, 0.0f, 1.0f);
        return Vector2{
            innerMinX + normX * innerW,
            innerMaxY - normZ * innerH
        };
    };

    // 9. ミニマップ上の敵スポーン警戒ゾーン（円＆外枠リング）の描画
    if (spawnMgr_) {
        const auto& spawnPoints = spawnMgr_->GetCachedSpawnPoints();
        size_t enemyCircleIdx = 0;
        constexpr float kMapEnemyFillAlpha = 0.28f;
        constexpr float kMapRingThickness = 1.5f;
        constexpr float kMapEnemyRingAlpha = 0.85f;
        constexpr float kMinMapCircleRadius = 5.0f;

        for (auto* sp : spawnPoints) {
            if (!sp || sp->GetTargetArea() != currentArea_) continue;
            // 走行フェーズ中で敵出現済みの場合はミニマップ上でも非表示（制圧済み）
            if (currentPhase_ == Phase::Play && sp->HasTriggered()) continue;

            auto* ownerGo = sp->GetOwner();
            if (!ownerGo) continue;

            Vector3 ownerPos = ownerGo->GetPosition();
            Vector2 spMapPos = WorldToMap2D(ownerPos);

            // スポーン・感知半径のミニマップ内スケール変換 (XZ比率に基づくピクセル半径)
            float radiusMeters = sp->GetEffectiveRadius();
            float radius2D = (radiusMeters / (2.0f * kMapBoundaryX)) * innerW;
            if (radius2D < kMinMapCircleRadius) radius2D = kMinMapCircleRadius;

            // プール拡張
            while (mapEnemyCircles_.size() <= enemyCircleIdx) {
                auto circle = std::make_unique<Circle2DObject>();
                circle->Initialize();
                SceneHierarchy::GetInstance()->Unregister(circle.get());
                mapEnemyCircles_.push_back(std::move(circle));
            }
            while (mapEnemyRings_.size() <= enemyCircleIdx) {
                auto ring = std::make_unique<Ring2DObject>();
                ring->Initialize();
                SceneHierarchy::GetInstance()->Unregister(ring.get());
                mapEnemyRings_.push_back(std::move(ring));
            }

            auto theme = sp->GetVisualColorTheme();

            // 内側半透明サークル
            Vector4 fillColor = { theme.ringEditor.x, theme.ringEditor.y, theme.ringEditor.z, kMapEnemyFillAlpha };
            auto& circleObj = mapEnemyCircles_[enemyCircleIdx];
            circleObj->SetRadius(radius2D);
            circleObj->GetMaterialData()->color = fillColor;
            circleObj->SetPosition({ spMapPos.x - radius2D, spMapPos.y - radius2D, 0.0f });
            circleObj->Update();
            circleObj->Draw("white", true);

            // 外枠リング
            Vector4 ringColor = { theme.ringEditor.x, theme.ringEditor.y, theme.ringEditor.z, kMapEnemyRingAlpha };
            auto& ringObj = mapEnemyRings_[enemyCircleIdx];
            ringObj->SetRadii(radius2D, (std::max)(1.0f, radius2D - kMapRingThickness));
            ringObj->GetMaterialData()->color = ringColor;
            ringObj->SetPosition({ spMapPos.x - radius2D, spMapPos.y - radius2D, 0.0f });
            ringObj->Update();
            ringObj->Draw("white", true);

            enemyCircleIdx++;
        }
    }

    // 10. スタート地点・ゴール地点の描画
    Vector2 start2D = WorldToMap2D({ 0.0f, 0.0f, startZ });
    Vector2 goal2D = WorldToMap2D({ 0.0f, 0.0f, goalZ });

    startCircle_->SetPosition({ start2D.x - kCircleDrawRadius, start2D.y - kCircleDrawRadius, 0.0f });
    startCircle_->Update();
    startCircle_->Draw("white", true);

    goalCircle_->GetMaterialData()->color = hasReachedGoal ? kDxGoalConnectedColor : kDxGoalColor;
    goalCircle_->SetPosition({ goal2D.x - kCircleDrawRadius, goal2D.y - kCircleDrawRadius, 0.0f });
    goalCircle_->Update();
    goalCircle_->Draw("white", true);

    // 10. ゴール到達パルスリング
    if (hasReachedGoal) {
        pulseAnimTimer_ += 0.025f;
        float pulseProgress = std::fmod(pulseAnimTimer_ * kGoalPulseSpeed, 1.0f);
        float pulseRadius = kCircleDrawRadius + pulseProgress * kGoalPulseSpread;
        float pulseAlpha = (1.0f - pulseProgress) * (kGoalPulseMaxAlpha / 255.0f);

        goalPulseRing_->SetRadii(pulseRadius, (std::max)(1.0f, pulseRadius - 1.5f));
        goalPulseRing_->GetMaterialData()->color = { kDxGoalConnectedColor.x, kDxGoalConnectedColor.y, kDxGoalConnectedColor.z, pulseAlpha };
        goalPulseRing_->SetPosition({ goal2D.x - pulseRadius, goal2D.y - pulseRadius, 0.0f });
        goalPulseRing_->Update();
        goalPulseRing_->Draw("white", true);
    }

    // 11. 手書きルート線の描画
    if (hasReachedGoal) {
        const auto& pathPoints = routeMgr_->GetPathPoints();
        if (pathPoints.size() >= 2) {
            size_t segCount = pathPoints.size() - 1;
            while (routeLinePool_.size() < segCount) {
                auto line = std::make_unique<Line2DObject>();
                line->Initialize();
                SceneHierarchy::GetInstance()->Unregister(line.get());
                routeLinePool_.push_back(std::move(line));
            }
            for (size_t i = 0; i < segCount; ++i) {
                Vector2 p1 = WorldToMap2D(pathPoints[i]);
                Vector2 p2 = WorldToMap2D(pathPoints[i + 1]);
                auto& line = routeLinePool_[i];
                line->SetPoints(p1, p2);
                line->SetThickness(kCompletedLineThickness);
                line->GetMaterialData()->color = kDxCompletedRouteColor;
                line->Update();
                line->Draw("white", true);
            }
        }
    } else {
        const auto& rawPoints = routeMgr_->GetRawPoints();
        if (rawPoints.size() >= 2) {
            size_t segCount = rawPoints.size() - 1;
            while (routeLinePool_.size() < segCount) {
                auto line = std::make_unique<Line2DObject>();
                line->Initialize();
                SceneHierarchy::GetInstance()->Unregister(line.get());
                routeLinePool_.push_back(std::move(line));
            }
            for (size_t i = 0; i < segCount; ++i) {
                Vector2 p1 = WorldToMap2D(rawPoints[i]);
                Vector2 p2 = WorldToMap2D(rawPoints[i + 1]);
                auto& line = routeLinePool_[i];
                line->SetPoints(p1, p2);
                line->SetThickness(kRouteLineThickness);
                line->GetMaterialData()->color = kDxDrawingRouteColor;
                line->Update();
                line->Draw("white", true);
            }
        }
    }

    // 12. 自機位置アイコンの描画（Playフェーズ時）
    if (currentPhase_ == Phase::Play && routeMgr_) {
        Vector3 playerPos = routeMgr_->GetPositionAtDistance(currentDistance_);
        Vector2 player2D = WorldToMap2D(playerPos);
        playerIconCircle_->SetPosition({ player2D.x - kPlayerIconRadius, player2D.y - kPlayerIconRadius, 0.0f });
        playerIconCircle_->Update();
        playerIconCircle_->Draw("white", true);
    }
}

void GamePhaseControllerScript::RenderGoSignDirectX() {
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    float targetMouseX = 0.0f;
    float targetMouseY = 0.0f;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        targetMouseX = mousePos.x * (kRenderWidth / viewSize.x);
        targetMouseY = mousePos.y * (kRenderHeight / viewSize.y);
    }

    // ボタンの領域計算（画面中央下部）
    float goBtnMinX = (kRenderWidth - kGoButtonWidth) * 0.5f;
    float goBtnMinY = kRenderHeight - kGoButtonMarginBottom - kGoButtonHeight;
    float goBtnMaxX = goBtnMinX + kGoButtonWidth;
    float goBtnMaxY = goBtnMinY + kGoButtonHeight;

    float resetBtnMinX = (kRenderWidth - kResetButtonWidth) * 0.5f;
    float resetBtnMinY = goBtnMinY - kResetButtonHeight - kResetBtnSpacing;
    float resetBtnMaxX = resetBtnMinX + kResetButtonWidth;
    float resetBtnMaxY = resetBtnMinY + kResetButtonHeight;

    bool isHoverReset = (targetMouseX >= resetBtnMinX && targetMouseX <= resetBtnMaxX &&
                         targetMouseY >= resetBtnMinY && targetMouseY <= resetBtnMaxY);
    bool isHoverGo = (targetMouseX >= goBtnMinX && targetMouseX <= goBtnMaxX &&
                      targetMouseY >= goBtnMinY && targetMouseY <= goBtnMaxY);

    // 1. リセットボタン描画
    resetBtnSprite_->SetPosition({ resetBtnMinX, resetBtnMinY, 0.0f });
    resetBtnSprite_->SetSize(kResetButtonWidth, kResetButtonHeight);
    resetBtnSprite_->GetMaterialData()->color = isHoverReset ? kDxResetBtnHoverBg : kDxResetBtnNormalBg;
    resetBtnSprite_->Update();
    resetBtnSprite_->Draw("white", true);

    float rTextX = resetBtnMinX + (kResetButtonWidth - resetBtnTextGen_->GetWidth()) * 0.5f;
    float rTextY = resetBtnMinY + (kResetButtonHeight - resetBtnTextGen_->GetHeight()) * 0.5f;
    resetBtnTextObj_->SetPosition({ rTextX, rTextY, 0.0f });
    resetBtnTextObj_->Update();
    resetBtnTextObj_->Draw(resetBtnTextGen_->GetGpuHandle(), true);

    // 2. メイン GO ボタン描画
    goBtnSprite_->SetPosition({ goBtnMinX, goBtnMinY, 0.0f });
    goBtnSprite_->SetSize(kGoButtonWidth, kGoButtonHeight);
    goBtnSprite_->GetMaterialData()->color = isHoverGo ? kDxGoBtnHoverBg : kDxGoBtnNormalBg;
    goBtnSprite_->Update();
    goBtnSprite_->Draw("white", true);

    float gTextX = goBtnMinX + (kGoButtonWidth - goBtnTextGen_->GetWidth()) * 0.5f;
    float gTextY = goBtnMinY + (kGoButtonHeight - goBtnTextGen_->GetHeight()) * 0.5f;
    goBtnTextObj_->SetPosition({ gTextX, gTextY, 0.0f });
    goBtnTextObj_->Update();
    goBtnTextObj_->Draw(goBtnTextGen_->GetGpuHandle(), true);
}

/// <summary>
/// シーン上に存在するすべての敵キャラクターおよび弾丸オブジェクトを探索し、
/// 非表示化およびシーンからの安全な削除（DestroyGameObject）を実行します。
/// </summary>
void GamePhaseControllerScript::ClearAllEnemies() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    std::vector<GameObject*> toDestroy;
    for (const auto& obj : scene->GetGameObjects()) {
        if (!obj) continue;
        // 敵AIまたは弾丸スクリプトを持つオブジェクトを消去対象とする
        if (obj->GetComponent<EnemyAIScript>() || obj->GetComponent<BulletScript>()) {
            obj->SetVisible(false);
            toDestroy.push_back(obj.get());
        }
    }

    for (auto* obj : toDestroy) {
        scene->DestroyGameObject(obj);
    }
}

/// <summary>
/// レティクル（照準UI）オブジェクトの表示/非表示状態を切り替えます。
/// </summary>
/// <param name="visible">表示する場合 true</param>
void GamePhaseControllerScript::SetReticleVisible(bool visible) {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    for (const auto& obj : scene->GetGameObjects()) {
        if (!obj) continue;
        if (obj->GetName() == kReticleObjectName || obj->GetComponent<ReticleControllerScript>()) {
            obj->SetVisible(visible);
        }
    }
}

/// <summary>
/// 走行・戦闘フェーズ（Play）中に画面端に自機HPバー、残弾数、リロード状態、エリア進捗をDirectX 12で描画します。
/// </summary>
void GamePhaseControllerScript::RenderPlayHUDDirectX() {
    if (!playerScript_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    // 1. 自機HPパネル描画（画面左下）
    float hpPanelX = kHudHpMarginLeft;
    float hpPanelY = kRenderHeight - kHudHpMarginBottom - kHudHpPanelHeight;

    // パネル背景
    hudHpBgSprite_->SetPosition({ hpPanelX, hpPanelY, 0.0f });
    hudHpBgSprite_->SetSize(kHudHpPanelWidth, kHudHpPanelHeight);
    hudHpBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
    hudHpBgSprite_->Update();
    hudHpBgSprite_->Draw("white", true);

    // パネル外枠4辺
    hudHpBorders_[0]->SetPoints({ hpPanelX, hpPanelY }, { hpPanelX + kHudHpPanelWidth, hpPanelY });
    hudHpBorders_[1]->SetPoints({ hpPanelX, hpPanelY + kHudHpPanelHeight }, { hpPanelX + kHudHpPanelWidth, hpPanelY + kHudHpPanelHeight });
    hudHpBorders_[2]->SetPoints({ hpPanelX, hpPanelY }, { hpPanelX, hpPanelY + kHudHpPanelHeight });
    hudHpBorders_[3]->SetPoints({ hpPanelX + kHudHpPanelWidth, hpPanelY }, { hpPanelX + kHudHpPanelWidth, hpPanelY + kHudHpPanelHeight });
    for (int i = 0; i < 4; ++i) {
        hudHpBorders_[i]->Update();
        hudHpBorders_[i]->Draw("white", true);
    }

    // HPゲージ背景
    float barX = hpPanelX + kHudHpBarPadding;
    float barY = hpPanelY + kHudHpPanelHeight - kHudHpBarPadding - kHudHpBarHeight;
    float barFullWidth = kHudHpPanelWidth - kHudHpBarPadding * 2.0f;
    hudHpBarBgSprite_->SetPosition({ barX, barY, 0.0f });
    hudHpBarBgSprite_->SetSize(barFullWidth, kHudHpBarHeight);
    hudHpBarBgSprite_->Update();
    hudHpBarBgSprite_->Draw("white", true);

    // HPゲージ本体
    int hp = playerScript_->GetHp();
    int maxHp = playerScript_->GetMaxHp();
    float hpRatio = (maxHp > 0) ? std::clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f) : 0.0f;
    float barCurrentWidth = barFullWidth * hpRatio;
    if (barCurrentWidth > 0.0f) {
        hudHpBarFillSprite_->SetPosition({ barX, barY, 0.0f });
        hudHpBarFillSprite_->SetSize(barCurrentWidth, kHudHpBarHeight);
        Vector4 hpColor = (hpRatio > 0.5f) ? kDxHpGreen : ((hpRatio > 0.25f) ? kDxHpYellow : kDxHpRed);
        hudHpBarFillSprite_->GetMaterialData()->color = hpColor;
        hudHpBarFillSprite_->Update();
        hudHpBarFillSprite_->Draw("white", true);
    }

    // HPテキスト
    char hpBuf[32];
    snprintf(hpBuf, sizeof(hpBuf), "HP %d / %d", hp, maxHp);
    if (hudHpTextGen_->Generate(hpBuf, "Yu Gothic UI", 12.0f, 0, true, false, false)) {
        hudHpTextObj_->SetSize(static_cast<float>(hudHpTextGen_->GetWidth()), static_cast<float>(hudHpTextGen_->GetHeight()));
    }
    hudHpTextObj_->SetPosition({ barX, hpPanelY + 5.0f, 0.0f });
    hudHpTextObj_->SetColor(kDxTextColor);
    hudHpTextObj_->Update();
    hudHpTextObj_->Draw(hudHpTextGen_->GetGpuHandle(), true);

    // 2. 残弾数パネル描画（画面右下）
    float ammoPanelX = kRenderWidth - kHudAmmoMarginRight - kHudAmmoPanelWidth;
    float ammoPanelY = kRenderHeight - kHudAmmoMarginBottom - kHudAmmoPanelHeight;

    hudAmmoBgSprite_->SetPosition({ ammoPanelX, ammoPanelY, 0.0f });
    hudAmmoBgSprite_->SetSize(kHudAmmoPanelWidth, kHudAmmoPanelHeight);
    hudAmmoBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
    hudAmmoBgSprite_->Update();
    hudAmmoBgSprite_->Draw("white", true);

    hudAmmoBorders_[0]->SetPoints({ ammoPanelX, ammoPanelY }, { ammoPanelX + kHudAmmoPanelWidth, ammoPanelY });
    hudAmmoBorders_[1]->SetPoints({ ammoPanelX, ammoPanelY + kHudAmmoPanelHeight }, { ammoPanelX + kHudAmmoPanelWidth, ammoPanelY + kHudAmmoPanelHeight });
    hudAmmoBorders_[2]->SetPoints({ ammoPanelX, ammoPanelY }, { ammoPanelX, ammoPanelY + kHudAmmoPanelHeight });
    hudAmmoBorders_[3]->SetPoints({ ammoPanelX + kHudAmmoPanelWidth, ammoPanelY }, { ammoPanelX + kHudAmmoPanelWidth, ammoPanelY + kHudAmmoPanelHeight });
    for (int i = 0; i < 4; ++i) {
        hudAmmoBorders_[i]->Update();
        hudAmmoBorders_[i]->Draw("white", true);
    }

    // 残弾テキスト
    bool isReloading = playerScript_->IsReloading();
    int ammo = playerScript_->GetAmmo();
    int maxAmmo = playerScript_->GetMaxAmmo();
    char ammoBuf[64];
    Vector4 ammoColor = kDxTextColor;

    if (isReloading) {
        snprintf(ammoBuf, sizeof(ammoBuf), "RELOADING...");
        ammoColor = kDxReloadingColor;
    } else {
        snprintf(ammoBuf, sizeof(ammoBuf), "AMMO: %d / %d", ammo, maxAmmo);
        ammoColor = (ammo <= 5) ? kDxAmmoAlert : kDxTextColor;
    }

    if (hudAmmoTextGen_->Generate(ammoBuf, "Yu Gothic UI", 13.0f, 1, true, false, false)) {
        hudAmmoTextObj_->SetSize(static_cast<float>(hudAmmoTextGen_->GetWidth()), static_cast<float>(hudAmmoTextGen_->GetHeight()));
    }
    float ammoTextX = ammoPanelX + (kHudAmmoPanelWidth - hudAmmoTextGen_->GetWidth()) * 0.5f;
    float ammoTextY = ammoPanelY + (kHudAmmoPanelHeight - hudAmmoTextGen_->GetHeight()) * 0.5f;
    hudAmmoTextObj_->SetPosition({ ammoTextX, ammoTextY, 0.0f });
    hudAmmoTextObj_->SetColor(ammoColor);
    hudAmmoTextObj_->Update();
    hudAmmoTextObj_->Draw(hudAmmoTextGen_->GetGpuHandle(), true);

    // 3. エリア進行度パネル描画（画面上部中央）
    float areaPanelX = (kRenderWidth - kHudAreaPanelWidth) * 0.5f;
    float areaPanelY = isCompassEnabled_ ? (kCompassBarY + kCompassBarHeight + 6.0f) : kHudAreaMarginTop;

    hudAreaBgSprite_->SetPosition({ areaPanelX, areaPanelY, 0.0f });
    hudAreaBgSprite_->SetSize(kHudAreaPanelWidth, kHudAreaPanelHeight);
    hudAreaBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
    hudAreaBgSprite_->Update();
    hudAreaBgSprite_->Draw("white", true);

    hudAreaBorders_[0]->SetPoints({ areaPanelX, areaPanelY }, { areaPanelX + kHudAreaPanelWidth, areaPanelY });
    hudAreaBorders_[1]->SetPoints({ areaPanelX, areaPanelY + kHudAreaPanelHeight }, { areaPanelX + kHudAreaPanelWidth, areaPanelY + kHudAreaPanelHeight });
    hudAreaBorders_[2]->SetPoints({ areaPanelX, areaPanelY }, { areaPanelX, areaPanelY + kHudAreaPanelHeight });
    hudAreaBorders_[3]->SetPoints({ areaPanelX + kHudAreaPanelWidth, areaPanelY }, { areaPanelX + kHudAreaPanelWidth, areaPanelY + kHudAreaPanelHeight });
    for (int i = 0; i < 4; ++i) {
        hudAreaBorders_[i]->Update();
        hudAreaBorders_[i]->Draw("white", true);
    }

    float totalLen = (routeMgr_) ? routeMgr_->GetTotalDistance() : 0.0f;
    float progress = (totalLen > 0.001f) ? std::clamp((currentDistance_ / totalLen) * 100.0f, 0.0f, 100.0f) : 0.0f;
    char areaBuf[64];
    snprintf(areaBuf, sizeof(areaBuf), "AREA %d/%d  [ %3.0f%% ]", currentArea_ + 1, kMaxAreas, progress);
    if (hudAreaTextGen_->Generate(areaBuf, "Yu Gothic UI", 13.0f, 1, true, false, false)) {
        hudAreaTextObj_->SetSize(static_cast<float>(hudAreaTextGen_->GetWidth()), static_cast<float>(hudAreaTextGen_->GetHeight()));
    }
    float areaTextX = areaPanelX + (kHudAreaPanelWidth - hudAreaTextGen_->GetWidth()) * 0.5f;
    float areaTextY = areaPanelY + (kHudAreaPanelHeight - hudAreaTextGen_->GetHeight()) * 0.5f;
    hudAreaTextObj_->SetPosition({ areaTextX, areaTextY, 0.0f });
    hudAreaTextObj_->SetColor(kDxTextColor);
    hudAreaTextObj_->Update();
    hudAreaTextObj_->Draw(hudAreaTextGen_->GetGpuHandle(), true);

    // 4. 画面上部コンパスHUD（方位バー）の描画
    if (isCompassEnabled_) {
        RenderCompassDirectX();
    }
}

/// <summary>
/// 走行戦闘中に画面上部中央へ水平スクロール式方位バー（自機進行方向基準のコンパスHUD）を描画します。
/// 自機の進行方向を「▲ FWD (0°)」とし、カメラの向いている角度に応じて目盛りと方位ラベルがリアルタイムにスクロールします。
/// </summary>
void GamePhaseControllerScript::RenderCompassDirectX() {
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    // 自機進行方向に対するカメラの相対Yaw角を算出
    Vector3 dir = playerScript_ ? playerScript_->GetDirection() : Vector3{ 0.0f, 0.0f, 1.0f };
    float baseYaw = std::atan2(dir.x, dir.z);
    float camYaw = camera->GetRotation().y;
    float diffYaw = camYaw - baseYaw;

    constexpr float kPi = 3.1415926535f;
    constexpr float kTwoPi = 6.283185307f;
    while (diffYaw > kPi) diffYaw -= kTwoPi;
    while (diffYaw < -kPi) diffYaw += kTwoPi;
    float relAngleDeg = diffYaw * (180.0f / kPi);

    // コンパスバーのスクリーン座標
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    float centerX = kRenderWidth * 0.5f;
    float barLeft = centerX - kCompassBarWidth * 0.5f;
    float barRight = centerX + kCompassBarWidth * 0.5f;
    float barTop = kCompassBarY;
    float barBottom = barTop + kCompassBarHeight;

    // 1. 背景描画
    if (compassBgSprite_) {
        compassBgSprite_->SetPosition({ barLeft, barTop, 0.0f });
        compassBgSprite_->SetSize(kCompassBarWidth, kCompassBarHeight);
        compassBgSprite_->Update();
        compassBgSprite_->Draw("white", true);
    }

    // 2. 枠線4辺描画
    if (compassBorders_[0]) {
        compassBorders_[0]->SetPoints({ barLeft, barTop }, { barRight, barTop });
        compassBorders_[1]->SetPoints({ barLeft, barBottom }, { barRight, barBottom });
        compassBorders_[2]->SetPoints({ barLeft, barTop }, { barLeft, barBottom });
        compassBorders_[3]->SetPoints({ barRight, barTop }, { barRight, barBottom });
        for (int i = 0; i < 4; ++i) {
            compassBorders_[i]->Update();
            compassBorders_[i]->Draw("white", true);
        }
    }

    // 3. 目盛り線描画 (15度刻み)
    float pxPerDegree = kCompassBarWidth / kCompassFovDegrees;
    size_t tickIndex = 0;
    for (int deg = -180; deg < 180; deg += 15) {
        float delta = static_cast<float>(deg) - relAngleDeg;
        while (delta > 180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;

        float screenX = centerX + delta * pxPerDegree;
        if (screenX >= barLeft + 4.0f && screenX <= barRight - 4.0f) {
            if (tickIndex < compassTickLines_.size()) {
                bool isMajor = (deg % 45 == 0);
                float tickH = isMajor ? kCompassTickMajorHeight : kCompassTickMinorHeight;
                auto& line = compassTickLines_[tickIndex++];
                line->SetPoints({ screenX, barBottom - tickH }, { screenX, barBottom });
                line->Update();
                line->Draw("white", true);
            }
        }
    }

    // 4. 方位ラベル描画 (8方位)
    for (auto& item : compassLabels_) {
        float delta = item.angleDeg - relAngleDeg;
        while (delta > 180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;

        float screenX = centerX + delta * pxPerDegree;
        if (screenX >= barLeft + 16.0f && screenX <= barRight - 16.0f) {
            float textX = screenX - item.textWidth * 0.5f;
            float textY = barTop + 2.0f;
            item.textObj->SetPosition({ textX, textY, 0.0f });
            item.textObj->SetColor(item.color);
            item.textObj->Update();
            item.textObj->Draw(item.textGen->GetGpuHandle(), true);
        }
    }

    // 5. センターマーカー描画 (中央視線インジケーター)
    if (compassCenterMarker_) {
        compassCenterMarker_->SetPoints({ centerX, barTop }, { centerX, barTop + 8.0f });
        compassCenterMarker_->Update();
        compassCenterMarker_->Draw("white", true);
    }
}

/// <summary>
/// ルート手書きフェーズ中に画面上部に案内ガイダンス（「線を引いてください」等）をDirectX 12で描画します。
/// </summary>
void GamePhaseControllerScript::RenderGuidanceDirectX() {
    if (!guidanceBgSprite_ || !guidanceTextObj_ || !guidanceTextGen_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    float panelX = (kRenderWidth - kGuidancePanelWidth) * 0.5f;
    float panelY = kGuidancePanelMarginTop;

    bool isGoal = (routeMgr_ && routeMgr_->HasReachedGoal());
    const std::string& currentPrompt = isGoal ? kGuidanceGoalPrompt : kGuidanceDrawPrompt;
    Vector4 borderColor = isGoal ? kDxPanelConnectedBorderColor : kDxGuidanceBorderColor;
    Vector4 textColor = isGoal ? kDxStartTextColor : kDxTextColor;

    // パネル背景描画
    guidanceBgSprite_->SetPosition({ panelX, panelY, 0.0f });
    guidanceBgSprite_->SetSize(kGuidancePanelWidth, kGuidancePanelHeight);
    guidanceBgSprite_->GetMaterialData()->color = kDxGuidanceBgColor;
    guidanceBgSprite_->Update();
    guidanceBgSprite_->Draw("white", true);

    // 外枠4辺描画
    guidanceBorders_[0]->SetPoints({ panelX, panelY }, { panelX + kGuidancePanelWidth, panelY });
    guidanceBorders_[1]->SetPoints({ panelX, panelY + kGuidancePanelHeight }, { panelX + kGuidancePanelWidth, panelY + kGuidancePanelHeight });
    guidanceBorders_[2]->SetPoints({ panelX, panelY }, { panelX, panelY + kGuidancePanelHeight });
    guidanceBorders_[3]->SetPoints({ panelX + kGuidancePanelWidth, panelY }, { panelX + kGuidancePanelWidth, panelY + kGuidancePanelHeight });
    for (int i = 0; i < 4; ++i) {
        guidanceBorders_[i]->GetMaterialData()->color = borderColor;
        guidanceBorders_[i]->Update();
        guidanceBorders_[i]->Draw("white", true);
    }

    // ガイダンステキスト描画（内容変化時のみ再生成）
    if (lastGuidanceText_ != currentPrompt) {
        lastGuidanceText_ = currentPrompt;
        guidanceTextGen_->Generate(currentPrompt, "Yu Gothic UI", 14.0f, 1, true, false, false);
        guidanceTextObj_->SetSize(static_cast<float>(guidanceTextGen_->GetWidth()), static_cast<float>(guidanceTextGen_->GetHeight()));
    }

    float textX = panelX + (kGuidancePanelWidth - guidanceTextGen_->GetWidth()) * 0.5f;
    float textY = panelY + (kGuidancePanelHeight - guidanceTextGen_->GetHeight()) * 0.5f;
    guidanceTextObj_->SetPosition({ textX, textY, 0.0f });
    guidanceTextObj_->SetColor(textColor);
    guidanceTextObj_->Update();
    guidanceTextObj_->Draw(guidanceTextGen_->GetGpuHandle(), true);
}

/// <summary>
/// 発進前カウントダウンフェーズ中に画面中央に大型カウントダウン数字をDirectX 12で描画します。
/// </summary>
void GamePhaseControllerScript::RenderCountdownDirectX() {
    if (!countdownBgSprite_ || !countdownTextObj_ || !countdownTextGen_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    // パルス倍率の計算（切り替わり時に大きく飛び出し、指数関数的に通常サイズへ収束）
    float pulseScale = 1.0f + (kPulseMaxScale - 1.0f) * std::exp(-pulseTimer_ * kPulseDecayRate);

    // フェード計算（秒数の終盤で次の秒数へ滑らかに遷移）
    float alphaFade = 1.0f;
    constexpr float kFadeThreshold = 0.18f;
    constexpr float kStartThreshold = 0.8f;

    if (countdownTimer_ > kStartThreshold) {
        float frac = (countdownTimer_ - kStartThreshold) - std::floor(countdownTimer_ - kStartThreshold);
        if (frac < kFadeThreshold && frac > 0.0f) {
            alphaFade = frac / kFadeThreshold;
        }
    }

    // 表示秒数判定
    std::string mainText;
    std::string subText;
    Vector4 textColor = kDxCountdownNumberColor;
    float baseFontSize = 130.0f;

    if (countdownTimer_ > kStartThreshold) {
        int sec = static_cast<int>(std::ceil(countdownTimer_ - kStartThreshold));
        mainText = std::to_string(sec);
        subText = kCountdownSubText;
        textColor = kDxCountdownNumberColor;
        baseFontSize = 130.0f;
    } else {
        mainText = kCountdownStartText;
        subText = kCountdownStartSubText;
        textColor = kDxStartTextColor;
        baseFontSize = 95.0f;
    }

    // テキスト内容が変化した瞬間のみテクスチャを1度だけ生成 (毎フレームのGDI再生成を完全排除)
    constexpr float kMainOutlineWidth = 6.0f;
    constexpr Vector4 kSolidBlack = { 0.0f, 0.0f, 0.0f, 1.0f };

    if (lastRenderedCountdownText_ != mainText) {
        lastRenderedCountdownText_ = mainText;
        countdownTextGen_->Generate(mainText, "Yu Gothic UI", baseFontSize, 1, true, false, true, kSolidBlack, kMainOutlineWidth, textColor);
        cachedCountdownTextWidth_ = static_cast<float>(countdownTextGen_->GetWidth());
        cachedCountdownTextHeight_ = static_cast<float>(countdownTextGen_->GetHeight());
    }

    constexpr float kSubFontSize = 22.0f;
    constexpr float kSubOutlineWidth = 3.5f;
    if (lastRenderedCountdownSubText_ != subText) {
        lastRenderedCountdownSubText_ = subText;
        countdownSubTextGen_->Generate(subText, "Yu Gothic UI", kSubFontSize, 1, true, false, true, kSolidBlack, kSubOutlineWidth, kDxTextColor);
        cachedCountdownSubWidth_ = static_cast<float>(countdownSubTextGen_->GetWidth());
        cachedCountdownSubHeight_ = static_cast<float>(countdownSubTextGen_->GetHeight());
    }

    // パルス拡縮とアルファフェードをGPUスプライト上で高速実行 (テクスチャ再生成ゼロ)
    float currentW = cachedCountdownTextWidth_ * pulseScale;
    float currentH = cachedCountdownTextHeight_ * pulseScale;
    float mainTextX = (kRenderWidth - currentW) * 0.5f;
    float mainTextY = (kRenderHeight * 0.5f) - (currentH * 0.58f);

    countdownTextObj_->SetSize(currentW, currentH);
    countdownTextObj_->SetPosition({ mainTextX, mainTextY, 0.0f });
    countdownTextObj_->SetColor({ 1.0f, 1.0f, 1.0f, alphaFade });
    countdownTextObj_->Update();
    countdownTextObj_->Draw(countdownTextGen_->GetGpuHandle(), true);

    // サブテキスト描画 (アウトライン付き、GPUフェード)
    float subTextX = (kRenderWidth - cachedCountdownSubWidth_) * 0.5f;
    float subTextY = mainTextY + currentH + 8.0f;
    countdownSubTextObj_->SetSize(cachedCountdownSubWidth_, cachedCountdownSubHeight_);
    countdownSubTextObj_->SetPosition({ subTextX, subTextY, 0.0f });
    countdownSubTextObj_->SetColor({ 1.0f, 1.0f, 1.0f, alphaFade });
    countdownSubTextObj_->Update();
    countdownSubTextObj_->Draw(countdownSubTextGen_->GetGpuHandle(), true);
}


