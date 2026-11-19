#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Route/RouteManagerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Enemy/EnemySpawnManagerScript.h"
#include "Scripts/Enemy/EnemySpawnPointScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Stage/MapRendererScript.h"
#include "Scripts/Stage/RestraintTrapScript.h"
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
    constexpr Vector4 kDxMapBuildingFillColor = { 45.0f / 255.0f, 60.0f / 255.0f, 85.0f / 255.0f, 215.0f / 255.0f };
    constexpr Vector4 kDxGridLineColor = { 40.0f / 255.0f, 70.0f / 255.0f, 95.0f / 255.0f, 120.0f / 255.0f };
    constexpr Vector4 kDxStartColor = { 40.0f / 255.0f, 240.0f / 255.0f, 100.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxGoalColor = { 255.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxGoalConnectedColor = { 40.0f / 255.0f, 240.0f / 255.0f, 140.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxDrawingRouteColor = { 255.0f / 255.0f, 180.0f / 255.0f, 30.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxCompletedRouteColor = { 20.0f / 255.0f, 220.0f / 255.0f, 255.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxDangerRouteColor = { 255.0f / 255.0f, 65.0f / 255.0f, 40.0f / 255.0f, 1.0f }; // 建物突入危険ルート線色 (鮮烈レッドオレンジ)
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

    // 拘束停止トラップ警告HUD用カラー (RGBA: 0.0f - 1.0f)
    constexpr Vector4 kDxRestraintBgColor = { 28.0f / 255.0f, 10.0f / 255.0f, 34.0f / 255.0f, 225.0f / 255.0f };
    constexpr Vector4 kDxRestraintBorderColor = { 255.0f / 255.0f, 55.0f / 255.0f, 130.0f / 255.0f, 245.0f / 255.0f };
    constexpr Vector4 kDxRestraintBarBgColor = { 18.0f / 255.0f, 8.0f / 255.0f, 22.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kDxRestraintBarFillColor = { 245.0f / 255.0f, 50.0f / 255.0f, 160.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kDxRestraintTitleColor = { 255.0f / 255.0f, 120.0f / 255.0f, 200.0f / 255.0f, 1.0f };
    constexpr Vector4 kDxRestraintTimerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr Vector4 kDxRestraintEdgeColor = { 255.0f / 255.0f, 45.0f / 255.0f, 125.0f / 255.0f, 0.70f };

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
    const std::string kGuidanceDrawPrompt = "左下の2Dマップをドラッグしてスタートからゴールまで線を引いてください";
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
    if (sInstance_ == this) {
        sInstance_ = nullptr;
    }
    // 2D DirectX12描画オブジェクト破棄時のクラッシュを防ぐためGPU処理を同期
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void GamePhaseControllerScript::Awake() {
    sInstance_ = this;
    currentPhase_ = Phase::DrawRoute;
    currentArea_ = 0;
    currentDistance_ = 0.0f;
    isSceneChanging_ = false;
    isLevelUpChoosing_ = false;
    levelUpInputGuardTimer_ = 0;
    hoveredCardIndex_ = -1;
    levelUpAnimTimer_ = 0.0f;
    currentUpgradeChoices_.clear();
    hudPopupTimer_ = 0;
    hudPopupText_.clear();
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
    // 新エリア開始時の黒フェードイン（暗転からフワッと明転）
    if (drawPhaseFadeAlpha_ > 0.0f) {
        drawPhaseFadeAlpha_ = (std::max)(0.0f, drawPhaseFadeAlpha_ - kBlackFadeSpeed * 1.5f);
    }

    auto* input = InputResource::GetInput();
    if (!input || !routeMgr_) {
        if (drawCamMode_ == DrawCameraMode::AutoOrbit) {
            UpdateDrawRouteCamera(false);
        }
        return;
    }

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        if (drawCamMode_ == DrawCameraMode::AutoOrbit) {
            UpdateDrawRouteCamera(false);
        }
        return;
    }

    // マウスがゲーム画面（GameViewWindow）上にない場合、または画面外領域の場合は入力を完全無視（自動見回しのみ継続）
    if (!GameViewWindow::IsGameViewVisible() || !GameViewWindow::IsMouseOnGameView() ||
        mousePos.x < 0.0f || mousePos.x > viewSize.x || mousePos.y < 0.0f || mousePos.y > viewSize.y) {
        if (drawCamMode_ == DrawCameraMode::AutoOrbit) {
            UpdateDrawRouteCamera(false);
        }
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

    // 画面右下の凡例パネルおよびカメラモード切替ボタン（中央GOボタンと重ならない左右対称配置）の領域計算
    float legendMinX = kRenderWidth - kLegendMarginRight - kLegendPanelWidth;
    float legendMinY = panelMaxY - kLegendPanelHeight;
    float legendMaxX = legendMinX + kLegendPanelWidth;
    float legendMaxY = panelMaxY;

    float camToggleMinX = legendMinX;
    float camToggleMinY = legendMinY - kCamToggleBtnGap - kCamToggleBtnHeight;
    float camToggleMaxX = camToggleMinX + kLegendPanelWidth;
    float camToggleMaxY = camToggleMinY + kCamToggleBtnHeight;

    // GOサインボタンの領域計算
    float goBtnMinX = (kRenderWidth - kGoButtonWidth) * 0.5f;
    float goBtnMinY = kRenderHeight - kGoButtonMarginBottom - kGoButtonHeight;
    float goBtnMaxX = goBtnMinX + kGoButtonWidth;
    float goBtnMaxY = goBtnMinY + kGoButtonHeight;

    float resetBtnMinX = (kRenderWidth - kResetButtonWidth) * 0.5f;
    float resetBtnMinY = goBtnMinY - kResetButtonHeight - kResetBtnSpacing;
    float resetBtnMaxX = resetBtnMinX + kResetButtonWidth;
    float resetBtnMaxY = resetBtnMinY + kResetButtonHeight;

    bool isMouseOver2DMapPanel = (targetMouseX >= panelMinX && targetMouseX <= panelMaxX &&
                                  targetMouseY >= panelMinY && targetMouseY <= panelMaxY);

    bool isMouseOver2DMap = (targetMouseX >= innerMinX && targetMouseX <= innerMaxX &&
                             targetMouseY >= innerMinY && targetMouseY <= innerMaxY);

    bool isMouseOverMapResetBtn = (targetMouseX >= mapResetBtnMinX && targetMouseX <= mapResetBtnMaxX &&
                                   targetMouseY >= mapResetBtnMinY && targetMouseY <= mapResetBtnMaxY);

    bool isMouseOverLegend = (targetMouseX >= legendMinX && targetMouseX <= legendMaxX &&
                              targetMouseY >= legendMinY && targetMouseY <= legendMaxY);

    bool isMouseOverCamToggleBtn = (targetMouseX >= camToggleMinX && targetMouseX <= camToggleMaxX &&
                                    targetMouseY >= camToggleMinY && targetMouseY <= camToggleMaxY);

    bool isMouseOverGoBtn = (routeMgr_->HasReachedGoal() &&
                             targetMouseX >= goBtnMinX && targetMouseX <= goBtnMaxX &&
                             targetMouseY >= goBtnMinY && targetMouseY <= goBtnMaxY);

    bool isMouseOverResetBtn = (routeMgr_->HasReachedGoal() &&
                                targetMouseX >= resetBtnMinX && targetMouseX <= resetBtnMaxX &&
                                targetMouseY >= resetBtnMinY && targetMouseY <= resetBtnMaxY);

    bool isTrigger = input->MouseTrigger(0);
    bool isPress = input->MousePress(0);
    bool isRightTrigger = input->MouseTrigger(1);

    // カメラモード切替（Cキー または 凡例上部のカメラ切替ボタンクリック）
    if (input->Trigger(DIK_C) || (isTrigger && isMouseOverCamToggleBtn)) {
        ToggleDrawCameraMode();
    }

    // 3Dマップカメラの更新（自動見回し ⇔ 自由操作）
    bool isOverAnyUI = (isMouseOver2DMapPanel || isMouseOverLegend || isMouseOverCamToggleBtn ||
                        isMouseOverGoBtn || isMouseOverResetBtn);
    if (drawCamMode_ == DrawCameraMode::AutoOrbit) {
        UpdateDrawRouteCamera(false);
    } else {
        UpdateFreeRoamCamera(!isOverAnyUI);
    }

    // 【常時リセット機能】
    // Rキー、2Dマップ上での右クリック、2Dマップ内RESETボタン、下部RESETボタンでリセット
    if (input->Trigger(DIK_R) || (isRightTrigger && isMouseOver2DMapPanel) ||
        (isTrigger && isMouseOverMapResetBtn) ||
        (isTrigger && isMouseOverResetBtn)) {
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

    // ルート手書きは2Dマップパネル内のみ受け付ける（3Dマップ上では線引きを行わない）
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
}

/// <summary>
/// 走行・戦闘フェーズ（Play）中の自機等速前進とゴール到達判定を処理します。
/// 走行距離 currentDistance_ を進行させ、ルートマネージャから求めた3D座標および接線方向を自機へ反映します。
/// </summary>
void GamePhaseControllerScript::UpdatePlayPhase() {
    // エリアクリア演出中の処理（カメラ背後引き・回り込み -> 飛翔プレイヤー見送り -> 空から文字降下 -> 暗転フェードアウト）
    if (isAreaClearing_) {
        areaClearTimer_++;

        Vector3 curCamPos = sendoffCamPos_;
        Vector3 currentTargetLookPos = goalPlayerPos_;

        // --- 第1幕: カメラが一人称視点から自機斜め後方へスムーズに後退・回り込み（0 〜 kCameraPullBackDuration: 約0.9秒）---
        if (areaClearTimer_ < kCameraPullBackDuration) {
            float camT = static_cast<float>(areaClearTimer_) / static_cast<float>(kCameraPullBackDuration);
            // SmoothStep で滑らかなカメラ後退補間
            float smoothT = camT * camT * (3.0f - 2.0f * camT);

            curCamPos = {
                areaClearInitialCamPos_.x + (sendoffCamPos_.x - areaClearInitialCamPos_.x) * smoothT,
                areaClearInitialCamPos_.y + (sendoffCamPos_.y - areaClearInitialCamPos_.y) * smoothT,
                areaClearInitialCamPos_.z + (sendoffCamPos_.z - areaClearInitialCamPos_.z) * smoothT
            };

            // 自機は地上でゆっくり微速走行しながら減速
            float moveSpeed = playerScript_ ? playerScript_->GetMoveSpeed() : playerMoveSpeed_;
            float slowStep = moveSpeed * kAreaClearSlowSpeedRatio * (1.0f - camT * 0.5f);
            goalPlayerPos_.x += goalPlayerDir_.x * slowStep;
            goalPlayerPos_.z += goalPlayerDir_.z * slowStep;
            goalPlayerPos_.y = kPlayerGroundYOffset;

            if (auto* playerGo = playerScript_->GetOwner()) {
                playerGo->SetPosition(goalPlayerPos_);
                playerGo->SetVisible(true);
            }
            playerScript_->SetDirection(goalPlayerDir_);

            // カメラ注視点: カメラが後退するにつれて前方注視から自機背中中心へ滑らかにシフト
            currentTargetLookPos = {
                goalPlayerPos_.x + goalPlayerDir_.x * (1.0f - smoothT) * 1.5f,
                goalPlayerPos_.y + 0.35f,
                goalPlayerPos_.z + goalPlayerDir_.z * (1.0f - smoothT) * 1.5f
            };
        }
        // --- 第2幕: 自機のロケット急上昇・見送り（kPlayerRocketStartFrame 〜 kPlayerRocketStartFrame + kPlayerRocketDuration: 約1.1秒）---
        else if (areaClearTimer_ < (kPlayerRocketStartFrame + kPlayerRocketDuration)) {
            int rocketElapsed = areaClearTimer_ - kPlayerRocketStartFrame;
            float jumpT = static_cast<float>(rocketElapsed) / static_cast<float>(kPlayerRocketDuration);
            // 放物線急加速上昇カーブ
            float easeY = jumpT * jumpT;

            // 前方へ駆け抜けながら急上昇！
            Vector3 flyPos = {
                goalPlayerPos_.x + goalPlayerDir_.x * (jumpT * kPlayerFlyForwardDist),
                kPlayerGroundYOffset + easeY * kPlayerFlyMaxHeight,
                goalPlayerPos_.z + goalPlayerDir_.z * (jumpT * kPlayerFlyForwardDist)
            };

            if (auto* playerGo = playerScript_->GetOwner()) {
                playerGo->SetPosition(flyPos);
                playerGo->SetVisible(true);
            }
            playerScript_->SetDirection(goalPlayerDir_);

            // 上昇中に自機足元から祝砲ブースタースパークを定期噴射
            if (rocketElapsed % 6 == 0) {
                if (auto effectMgr = EffectManager::GetInstance()) {
                    effectMgr->PlayGPUEffect({ flyPos.x, flyPos.y - 1.2f, flyPos.z }, 600);
                }
            }

            curCamPos = sendoffCamPos_;
            currentTargetLookPos = flyPos;
        }
        // --- 第3幕 & 第4幕: 自機消滅・見上げた天空から文字降下・暗転フェード（120F〜）---
        else {
            // 画面上空へ飛び去ったら、自機モデルを完全に非表示化！
            if (auto* playerGo = playerScript_->GetOwner()) {
                playerGo->SetVisible(false);
            }
            // 自機が消えた後も、カメラは飛び去った遥か上空を見上げた状態をキープ！
            curCamPos = sendoffCamPos_;
            currentTargetLookPos = {
                goalPlayerPos_.x + goalPlayerDir_.x * kPlayerFlyForwardDist,
                kPlayerGroundYOffset + kPlayerFlyMaxHeight,
                goalPlayerPos_.z + goalPlayerDir_.z * kPlayerFlyForwardDist
            };
        }

        // カメラ位置から注視対象への視線ベクトルと回転角 (Pitch, Yaw) を算出
        Vector3 toTarget = {
            currentTargetLookPos.x - curCamPos.x,
            currentTargetLookPos.y - curCamPos.y,
            currentTargetLookPos.z - curCamPos.z
        };
        float distXZ = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
        float yaw = std::atan2(toTarget.x, toTarget.z);
        float pitch = -std::atan2(toTarget.y, distXZ > 0.0001f ? distXZ : 0.0001f);

        // FPSCameraControllerScript および CameraComponent へ同期するための座標・回転を更新
        areaClearCurrentCamPos_ = curCamPos;
        areaClearCurrentCamRot_ = { pitch, yaw, 0.0f };

        // 内部カメラインスタンスへ即時適用
        if (auto* cameraMgr = CameraResource::GetCameraManager()) {
            if (BaseCamera* camera = cameraMgr->GetActiveCamera()) {
                camera->SetPosition(curCamPos);
                camera->SetRotation({ pitch, yaw, 0.0f });
            }
        }

        // ゴールドフラッシュのフェードアウト
        areaClearFlashAlpha_ = (std::max)(0.0f, areaClearFlashAlpha_ - kAreaClearFlashFadeSpeed * (1.0f / 60.0f));

        // 第4幕: 終盤の全画面黒フェードアウト（完全暗転トランジション: 準備前描画の完全隠蔽）
        if (areaClearTimer_ >= kFadeOutStartFrame) {
            transitionBlackFadeAlpha_ = (std::min)(1.0f, transitionBlackFadeAlpha_ + kBlackFadeSpeed);
        }

        // 演出完了（画面が100%真っ黒になった瞬間）に次エリア/クリアシーンへ安全移行！
        if (areaClearTimer_ >= kAreaClearDuration) {
            isAreaClearing_ = false;
            HandleAreaClear();
        }
        return;
    }

    // 発進・強化祝福フラッシュの減衰
    if (flashAlpha_ > 0.0f) {
        constexpr float kDeltaTime = 1.0f / 60.0f;
        flashAlpha_ = (std::max)(0.0f, flashAlpha_ - kFlashFadeSpeed * kDeltaTime);
    }

    // 霊魂吸収・レベルアップポップアップのタイマー更新
    if (hudPopupTimer_ > 0) {
        hudPopupTimer_--;
    }

    // Step 1-6: ダダサバイバー風 3択アップグレード選択中は戦闘進行を完全一時停止
    if (isLevelUpChoosing_) {
        UpdateLevelUpChoice();
        return;
    }

    // Step 1-6: 保留中のレベルアップがある場合は即座に3択アップグレード選択画面を開く
    if (playerScript_ && playerScript_->HasPendingLevelUp()) {
        OpenLevelUpChoice();
        return;
    }

    // 拘束停止円トラップ発動中は自機の自動前進を強制停止（FPS視点旋回・射撃・リロードは自由）
    if (isRestrained_) {
        restraintPulseTimer_ += kRestraintPulseSpeed;
        restraintTimerFrames_--;
        if (restraintTimerFrames_ <= 0) {
            ClearRestraint();
        }
    } else {
        float moveSpeed = playerScript_ ? playerScript_->GetEffectiveMoveSpeed() : playerMoveSpeed_;
        currentDistance_ += moveSpeed;
    }

    // 自機位置と向きをルートから更新 (足元を地面Y=0に接地)
    Vector3 playerPos = routeMgr_->GetPositionAtDistance(currentDistance_);
    Vector3 playerDir = routeMgr_->GetTangentAtDistance(currentDistance_);
    playerPos.y += kPlayerGroundYOffset;

    if (auto* playerGo = playerScript_->GetOwner()) {
        playerGo->SetPosition(playerPos);
    }
    playerScript_->SetDirection(playerDir);

    // 建物突入判定（視界悪化スモーク＆継続スリップダメージ）
    bool inBuilding = MapRendererScript::IsInsideBuilding(playerPos);
    if (inBuilding) {
        // スモークのフェードイン（視界を極端に狭める）
        buildingBlindAlpha_ = (std::min)(kBuildingBlindMaxAlpha, buildingBlindAlpha_ + kBuildingBlindFadeInSpeed);

        // 激突カメラシェイク（コンクリート壁を突き破る激しい振動）
        ScriptComponent::ShakeCamera(kBuildingCollisionShakeIntensity, kBuildingCollisionShakeDuration);

        // 周期スリップダメージ（ガリガリと削れる衝突ダメージ）
        buildingDamageTimer_++;
        if (buildingDamageTimer_ >= kBuildingDamageInterval) {
            buildingDamageTimer_ = 0;
            if (playerScript_) {
                playerScript_->TakeDamage(kBuildingDamageAmount);
            }
        }
    } else {
        // 建物外（道路）へ脱出時はスムーズに視界回復
        buildingBlindAlpha_ = (std::max)(0.0f, buildingBlindAlpha_ - kBuildingBlindFadeOutSpeed);
        buildingDamageTimer_ = 0;
    }

    // エリアゴール到達判定（即時切り替えず、カメラゆっくり背後回り込み＆自機見送り＆空から文字降下演出を開始！）
    if (currentDistance_ >= routeMgr_->GetTotalDistance()) {
        isAreaClearing_ = true;
        areaClearTimer_ = 0;
        areaClearFlashAlpha_ = 0.25f; // まばゆすぎず視界を邪魔しない控えめな祝福光

        // 前エリアの敵・弾を一括消去
        ClearAllEnemies();

        // 照準レティクルを非表示化（見送りシネマティック演出に集中）
        SetReticleVisible(false);

        // ゴール地点の自機位置と向きを確定・保存
        goalPlayerPos_ = playerPos;
        goalPlayerDir_ = playerDir;

        // ゴール到達時のカメラ現在位置を取得 (第1幕のスムーズ後退開始座標)
        if (auto* cameraMgr = CameraResource::GetCameraManager()) {
            if (BaseCamera* camera = cameraMgr->GetActiveCamera()) {
                areaClearInitialCamPos_ = camera->GetPosition();
                areaClearCurrentCamPos_ = camera->GetPosition();
                areaClearCurrentCamRot_ = camera->GetRotation();
            }
        }

        // 自機の右基底ベクトル（横オフセット用）
        Vector3 rightVec = { -goalPlayerDir_.z, 0.0f, goalPlayerDir_.x };
        float rightLen = std::sqrt(rightVec.x * rightVec.x + rightVec.z * rightVec.z);
        if (rightLen > 0.0001f) {
            rightVec.x /= rightLen;
            rightVec.z /= rightLen;
        }

        // 背後見送りカメラの位置を確定（自機後方 kSendoffCamBackDist、見上げる低さ kSendoffCamHeight）
        sendoffCamPos_ = {
            goalPlayerPos_.x - goalPlayerDir_.x * kSendoffCamBackDist + rightVec.x * kSendoffCamSideOffset,
            kSendoffCamHeight,
            goalPlayerPos_.z - goalPlayerDir_.z * kSendoffCamBackDist + rightVec.z * kSendoffCamSideOffset
        };

        // 祝砲GPUスパークを自機左右上空へ炸裂！
        if (auto effectMgr = EffectManager::GetInstance()) {
            Vector3 leftSparkPos = { playerPos.x - 4.5f, playerPos.y + 2.5f, playerPos.z + 6.0f };
            Vector3 rightSparkPos = { playerPos.x + 4.5f, playerPos.y + 2.5f, playerPos.z + 6.0f };
            effectMgr->PlayGPUEffect(leftSparkPos, 1500);
            effectMgr->PlayGPUEffect(rightSparkPos, 1500);
        }
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

    isAreaClearing_ = false;

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
/// 拘束停止円トラップ発動時に呼び出し、指定秒数あいだ自機の自動走行を強制停止します。
/// </summary>
void GamePhaseControllerScript::TriggerRestraint(float durationSeconds, bool isRushActive) {
    if (durationSeconds <= 0.0f) return;
    isRestrained_ = true;
    isRestraintRushActive_ = isRushActive;
    restraintDurationFrames_ = (std::max)(1, static_cast<int>(durationSeconds * kRestraintTargetFps));
    restraintTimerFrames_ = restraintDurationFrames_;
    restraintPulseTimer_ = 0.0f;
    lastRenderedRestraintTimerText_.clear();

    // 電磁拘束ロック時の衝撃カメラシェイク
    ScriptComponent::ShakeCamera(kRestraintLockShakeIntensity, kRestraintLockShakeDuration);
}

/// <summary>
/// 現在の拘束停止状態を強制解除します。
/// </summary>
void GamePhaseControllerScript::ClearRestraint() {
    isRestrained_ = false;
    isRestraintRushActive_ = false;
    restraintTimerFrames_ = 0;
    restraintDurationFrames_ = 0;
    restraintPulseTimer_ = 0.0f;
    lastRenderedRestraintTimerText_.clear();
}

float GamePhaseControllerScript::GetRestraintRemainingSeconds() const {
    if (!isRestrained_ || restraintTimerFrames_ <= 0) {
        return 0.0f;
    }
    return static_cast<float>(restraintTimerFrames_) / kRestraintTargetFps;
}

/// <summary>
/// ルート手書きフェーズ（DrawRoute）への遷移設定を行います。
/// 前エリアの残存敵・弾丸を完全消去し、照準レティクルを非表示化、自機走行を停止し、
/// カメラを斜め立体俯瞰の見下ろし画角へ切り替えます。
/// </summary>
void GamePhaseControllerScript::TransitionToDraw() {
    currentPhase_ = Phase::DrawRoute;
    isLevelUpChoosing_ = false;
    hoveredCardIndex_ = -1;
    currentUpgradeChoices_.clear();

    // 拘束状態および全拘束トラップをリセット
    ClearRestraint();
    RestraintTrapScript::ResetAllTraps();

    // 前エリアの敵・弾を完全消去し、照準レティクルを非表示化
    ClearAllEnemies();
    SetReticleVisible(false);

    // 自機（プレイヤー）の自動走行停止およびエリア開始地点への位置・向き初期化
    if (playerScript_) {
        playerScript_->SetAutoMoving(false);
        if (auto* playerGo = playerScript_->GetOwner()) {
            playerGo->SetVisible(true); // クリア演出で消滅した自機モデルを確実に再表示
            float startZ = routeMgr_ ? routeMgr_->GetCurrentAreaStartZ() : 0.0f;
            playerGo->SetPosition({ kDefaultPlayerPosX, kDefaultPlayerPosY + kPlayerGroundYOffset, startZ });
        }
        playerScript_->SetDirection(kDefaultPlayerForward);
    }

    // カメラをエリア自動パノラマ展望視点へ初期化
    drawCamMode_ = DrawCameraMode::AutoOrbit;
    drawCamOrbitTimer_ = 0.0f;
    UpdateDrawRouteCamera(true);

    // 新エリア開始時の黒フェードイン（暗転からフワッと明転）開始
    drawPhaseFadeAlpha_ = 1.0f;
    transitionBlackFadeAlpha_ = 0.0f;
}

/// <summary>
/// ルート手書きフェーズ（DrawRoute）中の3Dマップ自動パノラマ展望カメラを更新します。
/// スタート側（手前）から左右斜め上空へ優雅に往復スウィング＆前後ドリーし、
/// 2Dミニマップと方向感覚を保ったままエリア全体の地形・敵・トラップ配置を見渡せます。
/// </summary>
void GamePhaseControllerScript::UpdateDrawRouteCamera(bool pauseOrbit) {
    if (!routeMgr_) return;

    constexpr float kHalf = 0.5f;
    constexpr float kLookSlideFreqRatio = 0.65f;
    constexpr float kHeightWaveFreqRatio = 1.35f;
    constexpr float kMinDistEpsilon = 0.0001f;

    if (!pauseOrbit) {
        drawCamOrbitTimer_ += kDrawCamSwingSpeed;
    }

    float startZ = routeMgr_->GetCurrentAreaStartZ();
    float goalZ = routeMgr_->GetCurrentAreaGoalZ();
    float midZ = (startZ + goalZ) * kHalf;

    // スタート側（-Z）基準で左右 ±42度 の扇状パノラマ軌道（手前=スタート/奥=ゴールの空間認識を維持）
    float swingAngle = std::sin(drawCamOrbitTimer_) * kDrawCamMaxSwingAngle;
    Vector3 lookTarget = {
        0.0f,
        kDrawCamLookY,
        midZ + std::sin(drawCamOrbitTimer_ * kLookSlideFreqRatio) * kDrawCamLookZSlideAmp
    };

    Vector3 camPos = {
        lookTarget.x + std::sin(swingAngle) * kDrawCamOrbitRadius,
        kDrawCamBaseHeight + std::cos(drawCamOrbitTimer_ * kHeightWaveFreqRatio) * kDrawCamHeightAmp,
        lookTarget.z - std::cos(swingAngle) * kDrawCamOrbitRadius
    };

    Vector3 toTarget = {
        lookTarget.x - camPos.x,
        lookTarget.y - camPos.y,
        lookTarget.z - camPos.z
    };
    float distXZ = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    float yaw = std::atan2(toTarget.x, toTarget.z);
    float pitch = -std::atan2(toTarget.y, (distXZ > kMinDistEpsilon) ? distXZ : kMinDistEpsilon);

    drawCamCurrentPos_ = camPos;
    drawCamCurrentRot_ = { pitch, yaw, 0.0f };

    if (auto* cameraMgr = CameraResource::GetCameraManager()) {
        if (BaseCamera* camera = cameraMgr->GetActiveCamera()) {
            camera->SetPosition(drawCamCurrentPos_);
            camera->SetRotation(drawCamCurrentRot_);
        }
    }
}

/// <summary>
/// DrawRouteフェーズのカメラモード（自動見回し ⇔ 自由操作）をシームレスに切り替えます。
/// </summary>
void GamePhaseControllerScript::ToggleDrawCameraMode() {
    if (drawCamMode_ == DrawCameraMode::AutoOrbit) {
        drawCamMode_ = DrawCameraMode::FreeRoam;
        // 現在の自動見回し位置・回転をそのまま引き継いで自由操作開始
    } else {
        drawCamMode_ = DrawCameraMode::AutoOrbit;
    }
}

/// <summary>
/// ルート手書きフェーズ（DrawRoute）中の自由操作カメラを更新します。
/// 左ドラッグで視点回転、WASD/矢印キーで水平移動、Q/Eキーまたはマウスホイールで高度上下・ズームが行えます。
/// </summary>
void GamePhaseControllerScript::UpdateFreeRoamCamera(bool allowDragRotate) {
    auto* input = InputResource::GetInput();
    if (!input || !routeMgr_) return;

    constexpr float kEpsilonLen = 0.0001f;

    // 1. 左ドラッグによる視点の自由旋回（UIパネル外のみ）
    if (allowDragRotate && input->MousePress(0)) {
        float dx = input->GetMouseDeltaX();
        float dy = input->GetMouseDeltaY();
        drawCamCurrentRot_.y += dx * kFreeCamRotateSensitivity;
        drawCamCurrentRot_.x = std::clamp(
            drawCamCurrentRot_.x + dy * kFreeCamRotateSensitivity,
            kFreeCamMinPitch,
            kFreeCamMaxPitch
        );
    }

    // 2. WASD / 矢印キーによるカメラ水平移動（現在のカメラ向き基準）
    float forwardInput = 0.0f;
    float rightInput = 0.0f;
    if (input->Press(DIK_W) || input->Press(DIK_UP))    forwardInput += 1.0f;
    if (input->Press(DIK_S) || input->Press(DIK_DOWN))  forwardInput -= 1.0f;
    if (input->Press(DIK_D) || input->Press(DIK_RIGHT)) rightInput += 1.0f;
    if (input->Press(DIK_A) || input->Press(DIK_LEFT))  rightInput -= 1.0f;

    float inputLen = std::sqrt(forwardInput * forwardInput + rightInput * rightInput);
    if (inputLen > kEpsilonLen) {
        forwardInput /= inputLen;
        rightInput /= inputLen;

        float yaw = drawCamCurrentRot_.y;
        float fwdX = std::sin(yaw);
        float fwdZ = std::cos(yaw);
        float rightX = std::cos(yaw);
        float rightZ = -std::sin(yaw);

        drawCamCurrentPos_.x += (fwdX * forwardInput + rightX * rightInput) * kFreeCamMoveSpeed;
        drawCamCurrentPos_.z += (fwdZ * forwardInput + rightZ * rightInput) * kFreeCamMoveSpeed;
    }

    // 3. Q / E キー および マウスホイールによる高度上下・ズーム
    if (input->Press(DIK_E)) {
        drawCamCurrentPos_.y += kFreeCamVerticalSpeed;
    }
    if (input->Press(DIK_Q)) {
        drawCamCurrentPos_.y -= kFreeCamVerticalSpeed;
    }
    if (allowDragRotate) {
        float wheel = input->GetMouseWheel();
        if (std::abs(wheel) > kEpsilonLen) {
            // ホイール上転（正）で近づく（下降ズームイン）、下転（負）で遠ざかる（上昇ズームアウト）
            drawCamCurrentPos_.y -= wheel * kFreeCamWheelSpeed;
        }
    }

    // 4. マップを見失わないための安全範囲クランプ
    float startZ = routeMgr_->GetCurrentAreaStartZ();
    float goalZ = routeMgr_->GetCurrentAreaGoalZ();
    drawCamCurrentPos_.x = std::clamp(drawCamCurrentPos_.x, -kFreeCamBoundX, kFreeCamBoundX);
    drawCamCurrentPos_.y = std::clamp(drawCamCurrentPos_.y, kFreeCamMinHeight, kFreeCamMaxHeight);
    drawCamCurrentPos_.z = std::clamp(drawCamCurrentPos_.z, startZ - kFreeCamBoundZMargin, goalZ + kFreeCamBoundZMargin);

    // アクティブカメラへ即時反映
    if (auto* cameraMgr = CameraResource::GetCameraManager()) {
        if (BaseCamera* camera = cameraMgr->GetActiveCamera()) {
            camera->SetPosition(drawCamCurrentPos_);
            camera->SetRotation(drawCamCurrentRot_);
        }
    }
}

/// <summary>
/// 発進前カウントダウンフェーズ（Countdown）への遷移設定を行います。
/// ルートを確定し、自機をルート始点に配置・整列させ、発進カウントダウンを開始します。
/// </summary>
void GamePhaseControllerScript::TransitionToCountdown() {
    currentPhase_ = Phase::Countdown;
    ClearRestraint();
    RestraintTrapScript::ResetAllTraps();
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
            startPos.y += kPlayerGroundYOffset;
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
            startPos.y += kPlayerGroundYOffset;
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

void GamePhaseControllerScript::Draw() {
    // 全ビル群・床・敵などの不透明3D描画完了後に、半透明の銃口吸引ボルテックス＆マズル硝煙を描画する
    // （Game.jsonの登録順による「吸引リングがビルの裏に隠れる」描画順バグを防止）
    if (currentPhase_ == Phase::Play && !isLevelUpChoosing_ && playerScript_) {
        playerScript_->DrawVacuumVortex3D();
    }
}

void GamePhaseControllerScript::Draw2D() {
    if (!is2DInitialized_) {
        Init2DObjects();
    }

    // 共通ベクターアイコン描画プールのフレーム内カーソルをリセット
    iconLineCursor_ = 0;
    iconCircleCursor_ = 0;
    iconRingCursor_ = 0;
    iconSpriteCursor_ = 0;

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
        if (isLevelUpChoosing_) {
            // 3択選択中は戦闘用HUD(コンパス・エリア・HP・弾数)を隠し、
            // 暗転オーバーレイ＋見出し＆3択カード＋最上部EXPゲージ＆所持バフアイコンバーのみをスッキリ表示
            RenderLevelUpChoiceDirectX();
            RenderExpAndBuffBarDirectX();
        } else {
            // 通常戦闘中はプレイHUD、最上部EXPゲージ＆所持バフアイコンバー、拘束警告、吸収ポップアップを描画
            RenderPlayHUDDirectX();
            RenderExpAndBuffBarDirectX();

            if (isRestrained_ && !isAreaClearing_) {
                RenderRestraintWarningDirectX();
            }
            if (hudPopupTimer_ > 0) {
                RenderHudPopupDirectX();
            }
        }
    }

    // 4. 建物突入時の視界悪化スモーク描画（全画面暗転・粉塵オーバーレイ）
    if (buildingBlindSprite_ && buildingBlindAlpha_ > 0.005f) {
        constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
        constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
        buildingBlindSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        buildingBlindSprite_->SetSize(kRenderWidth, kRenderHeight);
        buildingBlindSprite_->GetMaterialData()->color = { 0.08f, 0.07f, 0.06f, buildingBlindAlpha_ };
        buildingBlindSprite_->Update();
        buildingBlindSprite_->Draw("white", true);
    }

    // 5. 発進白フラッシュ描画（全フェーズ共通の最前面オーバーレイ）
    if (flashSprite_ && flashAlpha_ > 0.005f) {
        constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
        constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
        flashSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        flashSprite_->SetSize(kRenderWidth, kRenderHeight);
        flashSprite_->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, flashAlpha_ };
        flashSprite_->Update();
        flashSprite_->Draw("white", true);
    }

    // 6. エリアクリア祝砲演出描画（ゴールドフラッシュ・バウンドバナー）
    if (isAreaClearing_ || areaClearFlashAlpha_ > 0.005f) {
        RenderAreaClearDirectX();
    }

    // 7. シームレス暗転黒フェード描画（準備前描画の完全隠蔽オーバーレイ）
    float blackAlpha = (std::max)(transitionBlackFadeAlpha_, drawPhaseFadeAlpha_);
    if (transitionBlackSprite_ && blackAlpha > 0.005f) {
        constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
        constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
        transitionBlackSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        transitionBlackSprite_->SetSize(kRenderWidth, kRenderHeight);
        transitionBlackSprite_->GetMaterialData()->color = { 0.0f, 0.0f, 0.0f, blackAlpha };
        transitionBlackSprite_->Update();
        transitionBlackSprite_->Draw("white", true);
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

    // 12-2. 建物突入時の視界悪化全画面スモークスプライト (DirectX 12)
    buildingBlindSprite_ = std::make_unique<SpriteObject>();
    buildingBlindSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(buildingBlindSprite_.get());
    buildingBlindSprite_->SetSize(kRenderWidth, kRenderHeight);
    buildingBlindSprite_->GetMaterialData()->color = { 0.08f, 0.07f, 0.06f, 0.0f };

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

    // 14. エリアクリア祝砲演出用パーツ (DirectX 12)
    areaClearBannerSprite_ = std::make_unique<SpriteObject>();
    areaClearBannerSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(areaClearBannerSprite_.get());
    areaClearBannerSprite_->SetSize(kAreaClearBannerWidth, kAreaClearBannerHeight);

    areaClearTextObj_ = std::make_unique<TextObject2D>();
    areaClearTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(areaClearTextObj_.get());
    areaClearTextGen_ = std::make_unique<TextTextureGenerator>();

    areaClearSubTextObj_ = std::make_unique<TextObject2D>();
    areaClearSubTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(areaClearSubTextObj_.get());
    areaClearSubTextGen_ = std::make_unique<TextTextureGenerator>();

    // 15. シームレス暗転フェード用スプライト (DirectX 12: 準備前描画の完全隠蔽オーバーレイ)
    transitionBlackSprite_ = std::make_unique<SpriteObject>();
    transitionBlackSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(transitionBlackSprite_.get());
    transitionBlackSprite_->SetSize(kRenderWidth, kRenderHeight);
    transitionBlackSprite_->GetMaterialData()->color = { 0.0f, 0.0f, 0.0f, 0.0f };

    // 16. 拘束停止円トラップ（Restraint Circle）警告HUDパーツ (DirectX 12)
    restraintPanelBgSprite_ = std::make_unique<SpriteObject>();
    restraintPanelBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(restraintPanelBgSprite_.get());
    restraintPanelBgSprite_->SetSize(kRestraintPanelWidth, kRestraintPanelHeight);

    for (int i = 0; i < 4; ++i) {
        restraintPanelBorders_[i] = std::make_unique<Line2DObject>();
        restraintPanelBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(restraintPanelBorders_[i].get());
        restraintPanelBorders_[i]->SetThickness(2.0f);

        restraintScreenEdges_[i] = std::make_unique<Line2DObject>();
        restraintScreenEdges_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(restraintScreenEdges_[i].get());
        restraintScreenEdges_[i]->SetThickness(kRestraintEdgeThickness);
    }

    restraintBarBgSprite_ = std::make_unique<SpriteObject>();
    restraintBarBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(restraintBarBgSprite_.get());

    restraintBarFillSprite_ = std::make_unique<SpriteObject>();
    restraintBarFillSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(restraintBarFillSprite_.get());

    restraintTitleTextObj_ = std::make_unique<TextObject2D>();
    restraintTitleTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(restraintTitleTextObj_.get());
    restraintTitleTextGen_ = std::make_unique<TextTextureGenerator>();
    if (restraintTitleTextGen_->Generate("! MOVEMENT LOCKED - 拘束中 !", "Yu Gothic UI", kRestraintTitleFontSize, 0, true, false, false)) {
        cachedRestraintTitleWidth_ = static_cast<float>(restraintTitleTextGen_->GetWidth());
        cachedRestraintTitleHeight_ = static_cast<float>(restraintTitleTextGen_->GetHeight());
        restraintTitleTextObj_->SetSize(cachedRestraintTitleWidth_, cachedRestraintTitleHeight_);
    }
    restraintRushTitleTextGen_ = std::make_unique<TextTextureGenerator>();
    if (restraintRushTitleTextGen_->Generate("! LOCKED - ENEMY RUSH 周囲警戒 !", "Yu Gothic UI", kRestraintTitleFontSize, 0, true, false, false)) {
        cachedRestraintRushTitleWidth_ = static_cast<float>(restraintRushTitleTextGen_->GetWidth());
        cachedRestraintRushTitleHeight_ = static_cast<float>(restraintRushTitleTextGen_->GetHeight());
    }

    restraintTimerTextObj_ = std::make_unique<TextObject2D>();
    restraintTimerTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(restraintTimerTextObj_.get());
    restraintTimerTextGen_ = std::make_unique<TextTextureGenerator>();

    // 17. 2Dミニマップ右側 ギミック一覧（GIMMICK LEGEND）パネルパーツ初期化
    legendBgSprite_ = std::make_unique<SpriteObject>();
    legendBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(legendBgSprite_.get());
    legendBgSprite_->SetSize(kLegendPanelWidth, kLegendPanelHeight);

    for (int i = 0; i < 4; ++i) {
        legendBorders_[i] = std::make_unique<Line2DObject>();
        legendBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(legendBorders_[i].get());
        legendBorders_[i]->SetThickness(1.5f);

        legendIconCircles_[i] = std::make_unique<Circle2DObject>();
        legendIconCircles_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(legendIconCircles_[i].get());
        legendIconCircles_[i]->SetRadius(kLegendIconRadius);
    }

    legendEnemyCoreDot_ = std::make_unique<Circle2DObject>();
    legendEnemyCoreDot_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(legendEnemyCoreDot_.get());
    legendEnemyCoreDot_->SetRadius(kMapEnemyCoreDotRadius);

    for (int i = 0; i < 2; ++i) {
        legendIconRings_[i] = std::make_unique<Ring2DObject>();
        legendIconRings_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(legendIconRings_[i].get());
    }

    legendTrapInnerRing_ = std::make_unique<Ring2DObject>();
    legendTrapInnerRing_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(legendTrapInnerRing_.get());

    legendBuildingIconSprite_ = std::make_unique<SpriteObject>();
    legendBuildingIconSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(legendBuildingIconSprite_.get());
    legendBuildingIconSprite_->SetSize(kLegendIconRadius * 2.0f, kLegendIconRadius * 2.0f);

    legendHeaderTextObj_ = std::make_unique<TextObject2D>();
    legendHeaderTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(legendHeaderTextObj_.get());
    legendHeaderTextGen_ = std::make_unique<TextTextureGenerator>();
    if (legendHeaderTextGen_->Generate("GIMMICK LEGEND (一覧)", "Yu Gothic UI", kLegendHeaderFontSize, 0, true, false, false)) {
        legendHeaderTextObj_->SetSize(
            static_cast<float>(legendHeaderTextGen_->GetWidth()),
            static_cast<float>(legendHeaderTextGen_->GetHeight()));
    }

    const char* kLegendLabels[kLegendItemCount] = {
        "敵エリア (敵が出現)",
        "拘束トラップ (一時停止)",
        "START (出発地点)",
        "GOAL (目的地)",
        "建物 (侵入でダメージ)"
    };
    for (int i = 0; i < kLegendItemCount; ++i) {
        legendItemTextObjs_[i] = std::make_unique<TextObject2D>();
        legendItemTextObjs_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(legendItemTextObjs_[i].get());
        legendItemTextGens_[i] = std::make_unique<TextTextureGenerator>();
        if (legendItemTextGens_[i]->Generate(kLegendLabels[i], "Yu Gothic UI", kLegendLabelFontSize, 0, true, false, false)) {
            float tw = static_cast<float>(legendItemTextGens_[i]->GetWidth());
            float th = static_cast<float>(legendItemTextGens_[i]->GetHeight());
            legendItemTextObjs_[i]->SetSize(tw, th);
            legendItemTextHeights_[i] = th;
        }
    }

    // 18. カメラモード切替ボタン＆自由カメラ操作ヒントUIパーツ初期化
    camToggleBtnSprite_ = std::make_unique<SpriteObject>();
    camToggleBtnSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(camToggleBtnSprite_.get());
    camToggleBtnSprite_->SetSize(kLegendPanelWidth, kCamToggleBtnHeight);

    freeCamHintBgSprite_ = std::make_unique<SpriteObject>();
    freeCamHintBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(freeCamHintBgSprite_.get());
    freeCamHintBgSprite_->SetSize(kLegendPanelWidth, kFreeCamHintHeight);

    for (int i = 0; i < 4; ++i) {
        camToggleBtnBorders_[i] = std::make_unique<Line2DObject>();
        camToggleBtnBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(camToggleBtnBorders_[i].get());
        camToggleBtnBorders_[i]->SetThickness(1.5f);

        freeCamHintBorders_[i] = std::make_unique<Line2DObject>();
        freeCamHintBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(freeCamHintBorders_[i].get());
        freeCamHintBorders_[i]->SetThickness(1.2f);
    }

    camToggleTextObj_ = std::make_unique<TextObject2D>();
    camToggleTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(camToggleTextObj_.get());

    camToggleTextAutoGen_ = std::make_unique<TextTextureGenerator>();
    if (camToggleTextAutoGen_->Generate("[C] カメラ: 自動見回し", "Yu Gothic UI", kCamToggleFontSize, 0, true, false, false)) {
        camToggleAutoW_ = static_cast<float>(camToggleTextAutoGen_->GetWidth());
        camToggleAutoH_ = static_cast<float>(camToggleTextAutoGen_->GetHeight());
    }
    camToggleTextFreeGen_ = std::make_unique<TextTextureGenerator>();
    if (camToggleTextFreeGen_->Generate("[C] カメラ: 自由操作", "Yu Gothic UI", kCamToggleFontSize, 0, true, false, false)) {
        camToggleFreeW_ = static_cast<float>(camToggleTextFreeGen_->GetWidth());
        camToggleFreeH_ = static_cast<float>(camToggleTextFreeGen_->GetHeight());
    }

    freeCamHintLine1Obj_ = std::make_unique<TextObject2D>();
    freeCamHintLine1Obj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(freeCamHintLine1Obj_.get());
    freeCamHintLine1Gen_ = std::make_unique<TextTextureGenerator>();
    if (freeCamHintLine1Gen_->Generate("左ドラッグ: 視点回転 / WASD: 移動", "Yu Gothic UI", kFreeCamHintFontSize, 0, true, false, false)) {
        freeCamHintLine1Obj_->SetSize(
            static_cast<float>(freeCamHintLine1Gen_->GetWidth()),
            static_cast<float>(freeCamHintLine1Gen_->GetHeight()));
    }

    freeCamHintLine2Obj_ = std::make_unique<TextObject2D>();
    freeCamHintLine2Obj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(freeCamHintLine2Obj_.get());
    freeCamHintLine2Gen_ = std::make_unique<TextTextureGenerator>();
    if (freeCamHintLine2Gen_->Generate("Q・E / ホイール: 高さ調整", "Yu Gothic UI", kFreeCamHintFontSize, 0, true, false, false)) {
        freeCamHintLine2Obj_->SetSize(
            static_cast<float>(freeCamHintLine2Gen_->GetWidth()),
            static_cast<float>(freeCamHintLine2Gen_->GetHeight()));
    }

    // 19. Step 1-6: 最上部EXPバー・LVバッジ・所持バフアイコンバー・右上ステータス要約・3択カード・ポップアップ初期化
    expBarBgSprite_ = std::make_unique<SpriteObject>();
    expBarBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(expBarBgSprite_.get());

    expBarFillSprite_ = std::make_unique<SpriteObject>();
    expBarFillSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(expBarFillSprite_.get());

    expBarBottomLine_ = std::make_unique<Line2DObject>();
    expBarBottomLine_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(expBarBottomLine_.get());
    expBarBottomLine_->SetThickness(1.5f);

    lvBadgeBgSprite_ = std::make_unique<SpriteObject>();
    lvBadgeBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(lvBadgeBgSprite_.get());
    lvExpBarBgSprite_ = std::make_unique<SpriteObject>();
    lvExpBarBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(lvExpBarBgSprite_.get());
    lvExpBarFillSprite_ = std::make_unique<SpriteObject>();
    lvExpBarFillSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(lvExpBarFillSprite_.get());
    lvBadgeTextObj_ = std::make_unique<TextObject2D>();
    lvBadgeTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(lvBadgeTextObj_.get());
    lvBadgeTextGen_ = std::make_unique<TextTextureGenerator>();

    statsPanelBgSprite_ = std::make_unique<SpriteObject>();
    statsPanelBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(statsPanelBgSprite_.get());
    statsPanelTextObj_ = std::make_unique<TextObject2D>();
    statsPanelTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(statsPanelTextObj_.get());
    statsPanelTextGen_ = std::make_unique<TextTextureGenerator>();

    for (int i = 0; i < 4; ++i) {
        lvBadgeBorders_[i] = std::make_unique<Line2DObject>();
        lvBadgeBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(lvBadgeBorders_[i].get());
        lvBadgeBorders_[i]->SetThickness(2.0f);

        statsPanelBorders_[i] = std::make_unique<Line2DObject>();
        statsPanelBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(statsPanelBorders_[i].get());
        statsPanelBorders_[i]->SetThickness(1.5f);

        levelUpHeaderBorders_[i] = std::make_unique<Line2DObject>();
        levelUpHeaderBorders_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(levelUpHeaderBorders_[i].get());
        levelUpHeaderBorders_[i]->SetThickness(2.5f);
    }

    for (int s = 0; s < kMaxBuffIconSlots; ++s) {
        buffSlotBgSprites_[s] = std::make_unique<SpriteObject>();
        buffSlotBgSprites_[s]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(buffSlotBgSprites_[s].get());

        for (int b = 0; b < 4; ++b) {
            buffSlotBorders_[s][b] = std::make_unique<Line2DObject>();
            buffSlotBorders_[s][b]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(buffSlotBorders_[s][b].get());
            buffSlotBorders_[s][b]->SetThickness(1.5f);
        }

        buffSlotLvTextObjs_[s] = std::make_unique<TextObject2D>();
        buffSlotLvTextObjs_[s]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(buffSlotLvTextObjs_[s].get());
        buffSlotLvTextGens_[s] = std::make_unique<TextTextureGenerator>();
    }

    levelUpDimSprite_ = std::make_unique<SpriteObject>();
    levelUpDimSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(levelUpDimSprite_.get());

    levelUpHeaderBgSprite_ = std::make_unique<SpriteObject>();
    levelUpHeaderBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(levelUpHeaderBgSprite_.get());

    levelUpHeaderTextObj_ = std::make_unique<TextObject2D>();
    levelUpHeaderTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(levelUpHeaderTextObj_.get());
    levelUpHeaderTextGen_ = std::make_unique<TextTextureGenerator>();

    levelUpSubTextObj_ = std::make_unique<TextObject2D>();
    levelUpSubTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(levelUpSubTextObj_.get());
    levelUpSubTextGen_ = std::make_unique<TextTextureGenerator>();

    for (int c = 0; c < kLevelUpCardCount; ++c) {
        choiceCardBgSprites_[c] = std::make_unique<SpriteObject>();
        choiceCardBgSprites_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardBgSprites_[c].get());

        choiceCardTopBandSprites_[c] = std::make_unique<SpriteObject>();
        choiceCardTopBandSprites_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardTopBandSprites_[c].get());

        choiceCardIconBoxSprites_[c] = std::make_unique<SpriteObject>();
        choiceCardIconBoxSprites_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardIconBoxSprites_[c].get());

        choiceCardBtnSprites_[c] = std::make_unique<SpriteObject>();
        choiceCardBtnSprites_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardBtnSprites_[c].get());

        for (int b = 0; b < 4; ++b) {
            choiceCardBorders_[c][b] = std::make_unique<Line2DObject>();
            choiceCardBorders_[c][b]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(choiceCardBorders_[c][b].get());
            choiceCardBorders_[c][b]->SetThickness(2.2f);

            choiceCardIconBoxBorders_[c][b] = std::make_unique<Line2DObject>();
            choiceCardIconBoxBorders_[c][b]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(choiceCardIconBoxBorders_[c][b].get());
            choiceCardIconBoxBorders_[c][b]->SetThickness(1.8f);
        }

        choiceCardCatTextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardCatTextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardCatTextObjs_[c].get());
        choiceCardCatTextGens_[c] = std::make_unique<TextTextureGenerator>();

        choiceCardTitleTextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardTitleTextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardTitleTextObjs_[c].get());
        choiceCardTitleTextGens_[c] = std::make_unique<TextTextureGenerator>();

        choiceCardLvTextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardLvTextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardLvTextObjs_[c].get());
        choiceCardLvTextGens_[c] = std::make_unique<TextTextureGenerator>();

        choiceCardDesc1TextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardDesc1TextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardDesc1TextObjs_[c].get());
        choiceCardDesc1TextGens_[c] = std::make_unique<TextTextureGenerator>();

        choiceCardDesc2TextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardDesc2TextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardDesc2TextObjs_[c].get());
        choiceCardDesc2TextGens_[c] = std::make_unique<TextTextureGenerator>();

        choiceCardKeyTextObjs_[c] = std::make_unique<TextObject2D>();
        choiceCardKeyTextObjs_[c]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(choiceCardKeyTextObjs_[c].get());
        choiceCardKeyTextGens_[c] = std::make_unique<TextTextureGenerator>();
    }

    iconLinePool_.reserve(kIconLinePoolSize);
    for (int i = 0; i < kIconLinePoolSize; ++i) {
        auto ln = std::make_unique<Line2DObject>();
        ln->Initialize();
        SceneHierarchy::GetInstance()->Unregister(ln.get());
        iconLinePool_.push_back(std::move(ln));
    }
    iconCirclePool_.reserve(kIconCirclePoolSize);
    for (int i = 0; i < kIconCirclePoolSize; ++i) {
        auto cr = std::make_unique<Circle2DObject>();
        cr->Initialize();
        SceneHierarchy::GetInstance()->Unregister(cr.get());
        iconCirclePool_.push_back(std::move(cr));
    }
    iconRingPool_.reserve(kIconRingPoolSize);
    for (int i = 0; i < kIconRingPoolSize; ++i) {
        auto rg = std::make_unique<Ring2DObject>();
        rg->Initialize();
        SceneHierarchy::GetInstance()->Unregister(rg.get());
        iconRingPool_.push_back(std::move(rg));
    }
    iconSpritePool_.reserve(kIconSpritePoolSize);
    for (int i = 0; i < kIconSpritePoolSize; ++i) {
        auto sp = std::make_unique<SpriteObject>();
        sp->Initialize();
        SceneHierarchy::GetInstance()->Unregister(sp.get());
        iconSpritePool_.push_back(std::move(sp));
    }

    hudPopupBgSprite_ = std::make_unique<SpriteObject>();
    hudPopupBgSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudPopupBgSprite_.get());

    hudPopupTextObj_ = std::make_unique<TextObject2D>();
    hudPopupTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(hudPopupTextObj_.get());
    hudPopupTextGen_ = std::make_unique<TextTextureGenerator>();

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

    // 9. ミニマップ上の市街地建物ブロック描画 (水平ランレングス結合で超軽量描画)
    size_t bldgSpriteIdx = 0;
    for (auto* mapRenderer : MapRendererScript::GetAllInstances()) {
        if (!mapRenderer || !mapRenderer->IsMapLoaded()) continue;

        const auto& mapGrid = mapRenderer->GetMapGrid();
        auto* mapOwner = mapRenderer->GetOwner();
        Vector3 basePos = mapOwner ? mapOwner->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };

        const float tileSize = mapGrid.tileSize;
        const float totalW = static_cast<float>(mapGrid.width) * tileSize;
        const float totalH = static_cast<float>(mapGrid.height) * tileSize;
        constexpr float kHalfFactor = 0.5f;
        const float startX = basePos.x - totalW * kHalfFactor;
        const float mapBaseZ = basePos.z - totalH * kHalfFactor;
        const float mapEndBaseZ = mapBaseZ + totalH;

        // このマップが現在のエリア区間 [startZ, goalZ] と重なっていない場合はスキップ
        if (mapEndBaseZ < startZ || mapBaseZ > goalZ) continue;

        for (int gz = 0; gz < mapGrid.height; ++gz) {
            float bldgMinZ = mapBaseZ + static_cast<float>(gz) * tileSize;
            float bldgMaxZ = bldgMinZ + tileSize;

            // 現在のエリアのZ区間 [startZ, goalZ] の外にあるマスはスキップ
            if (bldgMaxZ < startZ || bldgMinZ > goalZ) continue;

            int gx = 0;
            while (gx < mapGrid.width) {
                if (mapGrid.GetTile(gx, gz) == kTileBuilding) {
                    int startGx = gx;
                    while (gx < mapGrid.width && mapGrid.GetTile(gx, gz) == kTileBuilding) {
                        ++gx;
                    }
                    int endGx = gx; // 区間 [startGx, endGx)

                    float bldgMinX = startX + static_cast<float>(startGx) * tileSize;
                    float bldgMaxX = startX + static_cast<float>(endGx) * tileSize;

                    float normMinX = (bldgMinX + kMapBoundaryX) / (2.0f * kMapBoundaryX);
                    float normMaxX = (bldgMaxX + kMapBoundaryX) / (2.0f * kMapBoundaryX);
                    float normMinZ = (bldgMinZ - startZ) / areaLength;
                    float normMaxZ = (bldgMaxZ - startZ) / areaLength;

                    normMinX = std::clamp(normMinX, 0.0f, 1.0f);
                    normMaxX = std::clamp(normMaxX, 0.0f, 1.0f);
                    normMinZ = std::clamp(normMinZ, 0.0f, 1.0f);
                    normMaxZ = std::clamp(normMaxZ, 0.0f, 1.0f);

                    float rectMinX = innerMinX + normMinX * innerW;
                    float rectMaxX = innerMinX + normMaxX * innerW;
                    float rectMinY = innerMaxY - normMaxZ * innerH;
                    float rectMaxY = innerMaxY - normMinZ * innerH;

                    float rectW = rectMaxX - rectMinX;
                    float rectH = rectMaxY - rectMinY;

                    if (rectW > 0.0f && rectH > 0.0f) {
                        // プール拡張
                        while (mapBuildingSprites_.size() <= bldgSpriteIdx) {
                            auto sp = std::make_unique<SpriteObject>();
                            sp->Initialize();
                            SceneHierarchy::GetInstance()->Unregister(sp.get());
                            mapBuildingSprites_.push_back(std::move(sp));
                        }

                        auto& bldgSprite = mapBuildingSprites_[bldgSpriteIdx];
                        bldgSprite->SetPosition({ rectMinX, rectMinY, 0.0f });
                        bldgSprite->SetSize(rectW, rectH);
                        bldgSprite->GetMaterialData()->color = kDxMapBuildingFillColor;
                        bldgSprite->Update();
                        bldgSprite->Draw("white", true);

                        bldgSpriteIdx++;
                    }
                } else {
                    ++gx;
                }
            }
        }
    }

    // 10. ミニマップ上の敵スポーン警戒ゾーン（文字なし・中心ドット／ボスひし形◆の幾何学識別）
    if (spawnMgr_) {
        const auto& spawnPoints = spawnMgr_->GetCachedSpawnPoints();
        size_t enemyCircleIdx = 0;
        size_t enemyDotIdx = 0;
        size_t bossDiamondLineIdx = 0;
        constexpr float kMapEnemyFillAlpha = 0.24f;
        constexpr float kMapRingThickness = 1.6f;
        constexpr float kMapEnemyRingAlpha = 0.88f;
        constexpr float kMinMapCircleRadius = 6.0f;
        constexpr float kBossDiamondThickness = 1.8f;

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

            // 中心の幾何学アイコン（通常敵: ソリッドドット / ボス: ひし形◆）
            if (sp->IsBoss()) {
                while (mapBossDiamondLines_.size() < bossDiamondLineIdx + 4) {
                    auto line = std::make_unique<Line2DObject>();
                    line->Initialize();
                    SceneHierarchy::GetInstance()->Unregister(line.get());
                    mapBossDiamondLines_.push_back(std::move(line));
                }
                float d = (std::min)(kMapBossDiamondHalfSize, radius2D * 0.65f);
                Vector2 top = { spMapPos.x, spMapPos.y - d };
                Vector2 right = { spMapPos.x + d, spMapPos.y };
                Vector2 bottom = { spMapPos.x, spMapPos.y + d };
                Vector2 left = { spMapPos.x - d, spMapPos.y };
                const Vector2 ptsA[4] = { top, right, bottom, left };
                const Vector2 ptsB[4] = { right, bottom, left, top };
                for (int k = 0; k < 4; ++k) {
                    auto& seg = mapBossDiamondLines_[bossDiamondLineIdx + k];
                    seg->SetPoints(ptsA[k], ptsB[k]);
                    seg->SetThickness(kBossDiamondThickness);
                    seg->GetMaterialData()->color = theme.textColor;
                    seg->Update();
                    seg->Draw("white", true);
                }
                bossDiamondLineIdx += 4;
            } else {
                while (mapEnemyCoreDots_.size() <= enemyDotIdx) {
                    auto dot = std::make_unique<Circle2DObject>();
                    dot->Initialize();
                    SceneHierarchy::GetInstance()->Unregister(dot.get());
                    mapEnemyCoreDots_.push_back(std::move(dot));
                }
                auto& dotObj = mapEnemyCoreDots_[enemyDotIdx];
                dotObj->SetRadius(kMapEnemyCoreDotRadius);
                dotObj->GetMaterialData()->color = ringColor;
                dotObj->SetPosition({ spMapPos.x - kMapEnemyCoreDotRadius, spMapPos.y - kMapEnemyCoreDotRadius, 0.0f });
                dotObj->Update();
                dotObj->Draw("white", true);
                enemyDotIdx++;
            }

            enemyCircleIdx++;
        }
    }

    // 10-b. ミニマップ上の拘束停止円トラップ（ネオンパープル二重警告リング識別）
    {
        const auto& traps = RestraintTrapScript::GetActiveTraps();
        size_t trapIdx = 0;
        constexpr float kMapTrapFillAlpha = 0.28f;
        constexpr float kMapTrapRingAlpha = 0.95f;
        constexpr float kMapTrapInnerRingAlpha = 0.78f;
        constexpr float kMapTrapRingThickness = 1.8f;
        constexpr float kMapTrapInnerRingThickness = 1.3f;
        constexpr float kMinMapTrapRadius = 7.5f;

        for (auto* trap : traps) {
            if (!trap || trap->GetTargetArea() != currentArea_) continue;
            if (currentPhase_ == Phase::Play && trap->IsDisarmed()) continue;

            auto* ownerGo = trap->GetOwner();
            if (!ownerGo) continue;

            Vector3 trapPos = ownerGo->GetPosition();
            Vector2 trapMapPos = WorldToMap2D(trapPos);
            float radius2D = (trap->GetTrapRadius() / (2.0f * kMapBoundaryX)) * innerW;
            if (radius2D < kMinMapTrapRadius) radius2D = kMinMapTrapRadius;

            while (mapTrapCircles_.size() <= trapIdx) {
                auto circle = std::make_unique<Circle2DObject>();
                circle->Initialize();
                SceneHierarchy::GetInstance()->Unregister(circle.get());
                mapTrapCircles_.push_back(std::move(circle));
            }
            while (mapTrapRings_.size() <= trapIdx) {
                auto ring = std::make_unique<Ring2DObject>();
                ring->Initialize();
                SceneHierarchy::GetInstance()->Unregister(ring.get());
                mapTrapRings_.push_back(std::move(ring));
            }
            while (mapTrapInnerRings_.size() <= trapIdx) {
                auto innerRing = std::make_unique<Ring2DObject>();
                innerRing->Initialize();
                SceneHierarchy::GetInstance()->Unregister(innerRing.get());
                mapTrapInnerRings_.push_back(std::move(innerRing));
            }

            Vector4 baseColor = trap->GetMapColor();

            auto& circleObj = mapTrapCircles_[trapIdx];
            circleObj->SetRadius(radius2D);
            circleObj->GetMaterialData()->color = { baseColor.x, baseColor.y, baseColor.z, kMapTrapFillAlpha };
            circleObj->SetPosition({ trapMapPos.x - radius2D, trapMapPos.y - radius2D, 0.0f });
            circleObj->Update();
            circleObj->Draw("white", true);

            // 外周リング
            auto& ringObj = mapTrapRings_[trapIdx];
            ringObj->SetRadii(radius2D, (std::max)(1.0f, radius2D - kMapTrapRingThickness));
            ringObj->GetMaterialData()->color = { baseColor.x, baseColor.y, baseColor.z, kMapTrapRingAlpha };
            ringObj->SetPosition({ trapMapPos.x - radius2D, trapMapPos.y - radius2D, 0.0f });
            ringObj->Update();
            ringObj->Draw("white", true);

            // 内側二重警告リング
            float innerRadius = radius2D * kMapTrapInnerRingRatio;
            if (innerRadius > 2.0f) {
                auto& innerRingObj = mapTrapInnerRings_[trapIdx];
                innerRingObj->SetRadii(innerRadius, (std::max)(1.0f, innerRadius - kMapTrapInnerRingThickness));
                innerRingObj->GetMaterialData()->color = { baseColor.x, baseColor.y, baseColor.z, kMapTrapInnerRingAlpha };
                innerRingObj->SetPosition({ trapMapPos.x - innerRadius, trapMapPos.y - innerRadius, 0.0f });
                innerRingObj->Update();
                innerRingObj->Draw("white", true);
            }

            trapIdx++;
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

                // 建物内を通るセグメントは危険ルート警告色（赤橙色）で強調
                Vector3 midPoint = {
                    (pathPoints[i].x + pathPoints[i + 1].x) * 0.5f,
                    (pathPoints[i].y + pathPoints[i + 1].y) * 0.5f,
                    (pathPoints[i].z + pathPoints[i + 1].z) * 0.5f
                };
                bool isMidInBuilding = MapRendererScript::IsInsideBuilding(midPoint);
                line->GetMaterialData()->color = isMidInBuilding ? kDxDangerRouteColor : kDxCompletedRouteColor;

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

                // 手書き中も建物内セグメントは警告色
                Vector3 midPoint = {
                    (rawPoints[i].x + rawPoints[i + 1].x) * 0.5f,
                    (rawPoints[i].y + rawPoints[i + 1].y) * 0.5f,
                    (rawPoints[i].z + rawPoints[i + 1].z) * 0.5f
                };
                bool isMidInBuilding = MapRendererScript::IsInsideBuilding(midPoint);
                line->GetMaterialData()->color = isMidInBuilding ? kDxDangerRouteColor : kDxDrawingRouteColor;

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

    // 13. マップ右側にギミック一覧（LEGEND）パネルおよびカメラ切替ボタンを描画
    RenderMapLegendDirectX(panelMaxX, panelMaxY, targetMouseX, targetMouseY);
}

/// <summary>
/// 2Dミニマップの右側に配置するギミック一覧（LEGEND）パネルおよびカメラ切替UIを描画します。
/// </summary>
void GamePhaseControllerScript::RenderMapLegendDirectX(float mapPanelMaxX, float mapPanelMaxY, float targetMouseX, float targetMouseY) {
    if (!legendBgSprite_) return;

    constexpr float kHalf = 0.5f;
    constexpr float kHeaderTopPad = 7.0f;
    constexpr float kIconRingThickness = 1.6f;
    constexpr float kTrapInnerRingThickness = 1.1f;
    constexpr float kTextOffsetFromIcon = 12.0f;
    constexpr float kHintLine1PadY = 6.0f;
    constexpr float kHintLine2PadY = 22.0f;
    constexpr float kHintPadX = 10.0f;
    constexpr Vector4 kLegendEnemyFill = { 1.0f, 0.45f, 0.1f, 0.28f };
    constexpr Vector4 kLegendEnemyRing = { 1.0f, 0.45f, 0.1f, 0.95f };
    constexpr Vector4 kLegendTrapFill = { 0.88f, 0.22f, 1.0f, 0.28f };
    constexpr Vector4 kLegendTrapRing = { 0.88f, 0.22f, 1.0f, 0.95f };
    constexpr Vector4 kLegendTrapInnerRing = { 0.88f, 0.22f, 1.0f, 0.78f };
    constexpr Vector4 kCamAutoBg = { 18.0f / 255.0f, 45.0f / 255.0f, 70.0f / 255.0f, 230.0f / 255.0f };
    constexpr Vector4 kCamAutoHoverBg = { 30.0f / 255.0f, 75.0f / 255.0f, 115.0f / 255.0f, 245.0f / 255.0f };
    constexpr Vector4 kCamFreeBg = { 55.0f / 255.0f, 28.0f / 255.0f, 78.0f / 255.0f, 235.0f / 255.0f };
    constexpr Vector4 kCamFreeHoverBg = { 85.0f / 255.0f, 45.0f / 255.0f, 120.0f / 255.0f, 245.0f / 255.0f };
    constexpr Vector4 kCamFreeBorderColor = { 215.0f / 255.0f, 120.0f / 255.0f, 255.0f / 255.0f, 240.0f / 255.0f };

    (void)mapPanelMaxX;
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    float panelX = kRenderWidth - kLegendMarginRight - kLegendPanelWidth;
    float panelY = mapPanelMaxY - kLegendPanelHeight;

    // 1. 凡例パネル背景＆枠線
    legendBgSprite_->SetPosition({ panelX, panelY, 0.0f });
    legendBgSprite_->SetSize(kLegendPanelWidth, kLegendPanelHeight);
    legendBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
    legendBgSprite_->Update();
    legendBgSprite_->Draw("white", true);

    legendBorders_[0]->SetPoints({ panelX, panelY }, { panelX + kLegendPanelWidth, panelY });
    legendBorders_[1]->SetPoints({ panelX, panelY + kLegendPanelHeight }, { panelX + kLegendPanelWidth, panelY + kLegendPanelHeight });
    legendBorders_[2]->SetPoints({ panelX, panelY }, { panelX, panelY + kLegendPanelHeight });
    legendBorders_[3]->SetPoints({ panelX + kLegendPanelWidth, panelY }, { panelX + kLegendPanelWidth, panelY + kLegendPanelHeight });
    for (int i = 0; i < 4; ++i) {
        legendBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
        legendBorders_[i]->Update();
        legendBorders_[i]->Draw("white", true);
    }

    // 2. ヘッダーテキスト
    if (legendHeaderTextObj_ && legendHeaderTextGen_) {
        legendHeaderTextObj_->SetPosition({ panelX + kLegendPadding, panelY + kHeaderTopPad, 0.0f });
        legendHeaderTextObj_->SetColor(kDxPanelBorderColor);
        legendHeaderTextObj_->Update();
        legendHeaderTextObj_->Draw(legendHeaderTextGen_->GetGpuHandle(), true);
    }

    // 3. 各凡例行（ミニマップ実物と100%同じ幾何学アイコン＋ラベル）
    const Vector4 kCircleFillColors[4] = {
        kLegendEnemyFill,
        kLegendTrapFill,
        kDxStartColor,
        kDxGoalColor
    };
    const Vector4 kRingColors[2] = {
        kLegendEnemyRing,
        kLegendTrapRing
    };

    float iconCenterX = panelX + kLegendPadding + kLegendIconRadius;
    float textX = iconCenterX + kLegendIconRadius + kTextOffsetFromIcon;

    for (int i = 0; i < kLegendItemCount; ++i) {
        float rowCenterY = panelY + kLegendHeaderHeight + (static_cast<float>(i) + kHalf) * kLegendRowHeight;

        if (i < 4) {
            auto& circle = legendIconCircles_[i];
            circle->SetRadius(kLegendIconRadius);
            circle->GetMaterialData()->color = kCircleFillColors[i];
            circle->SetPosition({ iconCenterX - kLegendIconRadius, rowCenterY - kLegendIconRadius, 0.0f });
            circle->Update();
            circle->Draw("white", true);

            if (i < 2) {
                auto& ring = legendIconRings_[i];
                ring->SetRadii(kLegendIconRadius, (std::max)(1.0f, kLegendIconRadius - kIconRingThickness));
                ring->GetMaterialData()->color = kRingColors[i];
                ring->SetPosition({ iconCenterX - kLegendIconRadius, rowCenterY - kLegendIconRadius, 0.0f });
                ring->Update();
                ring->Draw("white", true);
            }

            // 敵エリア: 中心ソリッドドットを重ねる
            if (i == 0 && legendEnemyCoreDot_) {
                legendEnemyCoreDot_->SetRadius(kMapEnemyCoreDotRadius);
                legendEnemyCoreDot_->GetMaterialData()->color = kLegendEnemyRing;
                legendEnemyCoreDot_->SetPosition({ iconCenterX - kMapEnemyCoreDotRadius, rowCenterY - kMapEnemyCoreDotRadius, 0.0f });
                legendEnemyCoreDot_->Update();
                legendEnemyCoreDot_->Draw("white", true);
            }
            // 拘束トラップ: 内側二重リングを重ねる（ミニマップ実物と完全一致）
            else if (i == 1 && legendTrapInnerRing_) {
                float innerR = kLegendIconRadius * kMapTrapInnerRingRatio;
                legendTrapInnerRing_->SetRadii(innerR, (std::max)(1.0f, innerR - kTrapInnerRingThickness));
                legendTrapInnerRing_->GetMaterialData()->color = kLegendTrapInnerRing;
                legendTrapInnerRing_->SetPosition({ iconCenterX - innerR, rowCenterY - innerR, 0.0f });
                legendTrapInnerRing_->Update();
                legendTrapInnerRing_->Draw("white", true);
            }
        } else if (legendBuildingIconSprite_) {
            float boxSize = kLegendIconRadius * 2.0f;
            legendBuildingIconSprite_->SetSize(boxSize, boxSize);
            legendBuildingIconSprite_->GetMaterialData()->color = kDxMapBuildingFillColor;
            legendBuildingIconSprite_->SetPosition({ iconCenterX - kLegendIconRadius, rowCenterY - kLegendIconRadius, 0.0f });
            legendBuildingIconSprite_->Update();
            legendBuildingIconSprite_->Draw("white", true);
        }

        if (legendItemTextObjs_[i] && legendItemTextGens_[i]) {
            float textY = rowCenterY - legendItemTextHeights_[i] * kHalf;
            legendItemTextObjs_[i]->SetPosition({ textX, textY, 0.0f });
            legendItemTextObjs_[i]->SetColor(kDxTextColor);
            legendItemTextObjs_[i]->Update();
            legendItemTextObjs_[i]->Draw(legendItemTextGens_[i]->GetGpuHandle(), true);
        }
    }

    // 4. カメラモード切替トグルボタン（凡例パネルのすぐ上に配置）
    if (camToggleBtnSprite_) {
        float btnX = panelX;
        float btnY = panelY - kCamToggleBtnGap - kCamToggleBtnHeight;
        bool isHoverBtn = (targetMouseX >= btnX && targetMouseX <= btnX + kLegendPanelWidth &&
                           targetMouseY >= btnY && targetMouseY <= btnY + kCamToggleBtnHeight);
        bool isFree = (drawCamMode_ == DrawCameraMode::FreeRoam);

        Vector4 btnBg = isFree ? (isHoverBtn ? kCamFreeHoverBg : kCamFreeBg)
                               : (isHoverBtn ? kCamAutoHoverBg : kCamAutoBg);
        Vector4 btnBorder = isFree ? kCamFreeBorderColor : kDxPanelBorderColor;

        camToggleBtnSprite_->SetPosition({ btnX, btnY, 0.0f });
        camToggleBtnSprite_->SetSize(kLegendPanelWidth, kCamToggleBtnHeight);
        camToggleBtnSprite_->GetMaterialData()->color = btnBg;
        camToggleBtnSprite_->Update();
        camToggleBtnSprite_->Draw("white", true);

        camToggleBtnBorders_[0]->SetPoints({ btnX, btnY }, { btnX + kLegendPanelWidth, btnY });
        camToggleBtnBorders_[1]->SetPoints({ btnX, btnY + kCamToggleBtnHeight }, { btnX + kLegendPanelWidth, btnY + kCamToggleBtnHeight });
        camToggleBtnBorders_[2]->SetPoints({ btnX, btnY }, { btnX, btnY + kCamToggleBtnHeight });
        camToggleBtnBorders_[3]->SetPoints({ btnX + kLegendPanelWidth, btnY }, { btnX + kLegendPanelWidth, btnY + kCamToggleBtnHeight });
        for (int i = 0; i < 4; ++i) {
            camToggleBtnBorders_[i]->GetMaterialData()->color = btnBorder;
            camToggleBtnBorders_[i]->Update();
            camToggleBtnBorders_[i]->Draw("white", true);
        }

        TextTextureGenerator* btnGen = isFree ? camToggleTextFreeGen_.get() : camToggleTextAutoGen_.get();
        float textW = isFree ? camToggleFreeW_ : camToggleAutoW_;
        float textH = isFree ? camToggleFreeH_ : camToggleAutoH_;
        if (camToggleTextObj_ && btnGen && textW > 0.0f) {
            camToggleTextObj_->SetSize(textW, textH);
            camToggleTextObj_->SetPosition({ btnX + (kLegendPanelWidth - textW) * kHalf, btnY + (kCamToggleBtnHeight - textH) * kHalf, 0.0f });
            camToggleTextObj_->SetColor(kDxTextColor);
            camToggleTextObj_->Update();
            camToggleTextObj_->Draw(btnGen->GetGpuHandle(), true);
        }

        // 5. 自由カメラモード中のみ、操作ガイドパネルをボタンの上に表示
        if (isFree && freeCamHintBgSprite_) {
            float hintX = btnX;
            float hintY = btnY - kCamToggleBtnGap - kFreeCamHintHeight;

            freeCamHintBgSprite_->SetPosition({ hintX, hintY, 0.0f });
            freeCamHintBgSprite_->SetSize(kLegendPanelWidth, kFreeCamHintHeight);
            freeCamHintBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
            freeCamHintBgSprite_->Update();
            freeCamHintBgSprite_->Draw("white", true);

            freeCamHintBorders_[0]->SetPoints({ hintX, hintY }, { hintX + kLegendPanelWidth, hintY });
            freeCamHintBorders_[1]->SetPoints({ hintX, hintY + kFreeCamHintHeight }, { hintX + kLegendPanelWidth, hintY + kFreeCamHintHeight });
            freeCamHintBorders_[2]->SetPoints({ hintX, hintY }, { hintX, hintY + kFreeCamHintHeight });
            freeCamHintBorders_[3]->SetPoints({ hintX + kLegendPanelWidth, hintY }, { hintX + kLegendPanelWidth, hintY + kFreeCamHintHeight });
            for (int i = 0; i < 4; ++i) {
                freeCamHintBorders_[i]->GetMaterialData()->color = kCamFreeBorderColor;
                freeCamHintBorders_[i]->Update();
                freeCamHintBorders_[i]->Draw("white", true);
            }

            if (freeCamHintLine1Obj_ && freeCamHintLine1Gen_) {
                freeCamHintLine1Obj_->SetPosition({ hintX + kHintPadX, hintY + kHintLine1PadY, 0.0f });
                freeCamHintLine1Obj_->SetColor(kDxTextColor);
                freeCamHintLine1Obj_->Update();
                freeCamHintLine1Obj_->Draw(freeCamHintLine1Gen_->GetGpuHandle(), true);
            }
            if (freeCamHintLine2Obj_ && freeCamHintLine2Gen_) {
                freeCamHintLine2Obj_->SetPosition({ hintX + kHintPadX, hintY + kHintLine2PadY, 0.0f });
                freeCamHintLine2Obj_->SetColor(kDxSubTextColor);
                freeCamHintLine2Obj_->Update();
                freeCamHintLine2Obj_->Draw(freeCamHintLine2Gen_->GetGpuHandle(), true);
            }
        }
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

    // HPテキスト（数値は画面から隠し、"HP" ラベルのみ表示）
    if (hudHpTextGen_->Generate("HP", "Yu Gothic UI", 12.0f, 0, true, false, false)) {
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

    constexpr float kBlinkBase = 0.65f;
    constexpr float kBlinkAmp = 0.35f;
    constexpr float kReloadBlinkFreq = 8.0f;
    constexpr float kNoAmmoBlinkFreq = 10.0f;

    if (isReloading) {
        int rTimer = playerScript_->GetReloadTimer();
        int rDuration = playerScript_->GetReloadDuration();
        float reloadProgress = (rDuration > 0) ? std::clamp(1.0f - static_cast<float>(rTimer) / static_cast<float>(rDuration), 0.0f, 1.0f) : 0.0f;
        int reloadPercent = static_cast<int>(reloadProgress * 100.0f);
        snprintf(ammoBuf, sizeof(ammoBuf), "RELOADING... %d%%", reloadPercent);
        float blink = kBlinkBase + kBlinkAmp * std::sin(pulseTimer_ * kReloadBlinkFreq);
        ammoColor = { kDxReloadingColor.x, kDxReloadingColor.y, kDxReloadingColor.z, blink };
    } else if (ammo <= 0) {
        snprintf(ammoBuf, sizeof(ammoBuf), "EMPTY! [R] RELOAD");
        float blink = kBlinkBase + kBlinkAmp * std::sin(pulseTimer_ * kNoAmmoBlinkFreq);
        ammoColor = { 1.0f, 0.25f, 0.25f, blink }; // 警告レッド点滅
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

/// <summary>
/// エリア走破時の「AREA CLEAR!!」祝砲演出・バナー・ゴールドフラッシュをDirectX 12で描画します。
/// </summary>
void GamePhaseControllerScript::RenderAreaClearDirectX() {
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    constexpr float kPi = 3.14159265f;
    constexpr float kSubTextBlinkFreq = 12.0f;
    constexpr float kSubTextBlinkBase = 0.65f;
    constexpr float kSubTextBlinkAmp = 0.35f;

    // 1. 全画面ゴールデンフラッシュ（ゴールゲート通過時のまばゆい祝福の光）
    if (flashSprite_ && areaClearFlashAlpha_ > 0.005f) {
        flashSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        flashSprite_->SetSize(kRenderWidth, kRenderHeight);
        // 温かみのあるゴールデンフラッシュ (R=1.0, G=0.92, B=0.55)
        flashSprite_->GetMaterialData()->color = { 1.0f, 0.92f, 0.55f, areaClearFlashAlpha_ };
        flashSprite_->Update();
        flashSprite_->Draw("white", true);
    }

    if (!isAreaClearing_) return;

    // 自機が空にジャンプして消えるまでは文字を出さない
    if (areaClearTimer_ < kTextDropStartFrame) {
        return;
    }

    // 2. 「AREA CLEAR!!」中央バナー背景の上空滑空降下＆バウンド静止アニメーション
    int dropElapsed = areaClearTimer_ - kTextDropStartFrame;
    float dropT = (std::min)(1.0f, static_cast<float>(dropElapsed) / static_cast<float>(kTextDropDuration));

    float targetBannerY = (kRenderHeight * kAreaClearBannerTargetY) - (kAreaClearBannerHeight * 0.5f);
    float currentBannerY = targetBannerY;

    if (dropT < 0.70f) {
        // 上空（画面外 kAreaClearBannerDropStartY）から目標位置へ滑空降下 (イージング加速)
        float normT = dropT / 0.70f;
        float easeIn = normT * normT;
        currentBannerY = kAreaClearBannerDropStartY + (targetBannerY - kAreaClearBannerDropStartY) * easeIn;
    } else {
        // 目標位置到達時のポヨンとしたバウンド振動（減衰サイン波）
        float bounceNormT = (dropT - 0.70f) / 0.30f;
        float bounceOffset = std::sin(bounceNormT * kPi) * 18.0f * (1.0f - bounceNormT);
        currentBannerY = targetBannerY - bounceOffset;
    }

    float bannerW = kAreaClearBannerWidth;
    float bannerH = kAreaClearBannerHeight;
    float bannerX = (kRenderWidth - bannerW) * 0.5f;

    if (areaClearBannerSprite_) {
        areaClearBannerSprite_->SetPosition({ bannerX, currentBannerY, 0.0f });
        areaClearBannerSprite_->SetSize(bannerW, bannerH);
        areaClearBannerSprite_->GetMaterialData()->color = { 10.0f / 255.0f, 15.0f / 255.0f, 25.0f / 255.0f, 0.88f };
        areaClearBannerSprite_->Update();
        areaClearBannerSprite_->Draw("white", true);
    }

    // 3. 「AREA CLEAR!!」メインゴールドテキスト
    std::string mainText = (currentArea_ + 1 >= kMaxAreas) ? "ALL AREAS CLEARED!!" : ("AREA " + std::to_string(currentArea_ + 1) + " CLEAR!!");
    if (areaClearTextGen_ && areaClearTextObj_) {
        areaClearTextGen_->Generate(mainText, "Impact", 44.0f, 2, true, false, false);
        float tw = static_cast<float>(areaClearTextGen_->GetWidth());
        float th = static_cast<float>(areaClearTextGen_->GetHeight());
        float tx = (kRenderWidth - tw) * 0.5f;
        float ty = currentBannerY + 16.0f;

        areaClearTextObj_->SetPosition({ tx, ty, 0.0f });
        areaClearTextObj_->SetSize(tw, th);
        areaClearTextObj_->SetColor({ 1.0f, 0.88f, 0.20f, 1.0f }); // 黄金色
        areaClearTextObj_->Update();
        areaClearTextObj_->Draw(areaClearTextGen_->GetGpuHandle(), true);
    }

    // 4. 「NEXT AREA INITIALIZING...」サブテキスト（着地後に点滅表示）
    if (dropT >= 0.70f) {
        std::string subText = (currentArea_ + 1 >= kMaxAreas) ? "PREPARING VICTORY CEREMONY..." : "NEXT AREA INITIALIZING...";
        if (areaClearSubTextGen_ && areaClearSubTextObj_) {
            areaClearSubTextGen_->Generate(subText, "Yu Gothic UI", 16.0f, 1, true, false, false);
            float stw = static_cast<float>(areaClearSubTextGen_->GetWidth());
            float sth = static_cast<float>(areaClearSubTextGen_->GetHeight());
            float stx = (kRenderWidth - stw) * 0.5f;
            float sty = currentBannerY + bannerH - sth - 16.0f;

            // 点滅アニメーション
            float progress = static_cast<float>(areaClearTimer_) / static_cast<float>(kAreaClearDuration);
            float blink = kSubTextBlinkBase + kSubTextBlinkAmp * std::sin(progress * kSubTextBlinkFreq);
            areaClearSubTextObj_->SetPosition({ stx, sty, 0.0f });
            areaClearSubTextObj_->SetSize(stw, sth);
            areaClearSubTextObj_->SetColor({ 0.4f, 0.9f, 1.0f, blink }); // ネオンシアン
            areaClearSubTextObj_->Update();
            areaClearSubTextObj_->Draw(areaClearSubTextGen_->GetGpuHandle(), true);
        }
    }
}

/// <summary>
/// 拘束停止円トラップ発動中のHUDカウントダウン警告バナーおよび画面枠パルスを描画します。
/// </summary>
void GamePhaseControllerScript::RenderRestraintWarningDirectX() {
    if (!isRestrained_ || !restraintPanelBgSprite_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    constexpr float kHalf = 0.5f;
    constexpr float kPulseBase = 0.65f;
    constexpr float kPulseAmp = 0.35f;
    constexpr float kTitleTopOffset = 8.0f;
    constexpr float kTimerTopOffset = 30.0f;
    constexpr float kBarBottomMargin = 10.0f;
    constexpr float kEdgeInset = 2.0f;

    float pulse = kPulseBase + kPulseAmp * std::sin(restraintPulseTimer_);
    float remainRatio = (restraintDurationFrames_ > 0)
        ? std::clamp(static_cast<float>(restraintTimerFrames_) / static_cast<float>(restraintDurationFrames_), 0.0f, 1.0f)
        : 0.0f;

    // 1. 画面四辺のパルス警告フレーム（足止め状態を視界を遮らず伝達）
    Vector4 edgeColor = kDxRestraintEdgeColor;
    edgeColor.w *= pulse;
    restraintScreenEdges_[0]->SetPoints({ kEdgeInset, kEdgeInset }, { kRenderWidth - kEdgeInset, kEdgeInset });
    restraintScreenEdges_[1]->SetPoints({ kEdgeInset, kRenderHeight - kEdgeInset }, { kRenderWidth - kEdgeInset, kRenderHeight - kEdgeInset });
    restraintScreenEdges_[2]->SetPoints({ kEdgeInset, kEdgeInset }, { kEdgeInset, kRenderHeight - kEdgeInset });
    restraintScreenEdges_[3]->SetPoints({ kRenderWidth - kEdgeInset, kEdgeInset }, { kRenderWidth - kEdgeInset, kRenderHeight - kEdgeInset });
    for (int i = 0; i < 4; ++i) {
        restraintScreenEdges_[i]->GetMaterialData()->color = edgeColor;
        restraintScreenEdges_[i]->Update();
        restraintScreenEdges_[i]->Draw("white", true);
    }

    // 2. 画面上部中央の警告パネル背景＆点滅枠線
    float panelX = (kRenderWidth - kRestraintPanelWidth) * kHalf;
    float panelY = kRestraintPanelTopMargin;

    restraintPanelBgSprite_->SetPosition({ panelX, panelY, 0.0f });
    restraintPanelBgSprite_->SetSize(kRestraintPanelWidth, kRestraintPanelHeight);
    restraintPanelBgSprite_->GetMaterialData()->color = kDxRestraintBgColor;
    restraintPanelBgSprite_->Update();
    restraintPanelBgSprite_->Draw("white", true);

    Vector4 borderColor = kDxRestraintBorderColor;
    borderColor.w *= pulse;
    restraintPanelBorders_[0]->SetPoints({ panelX, panelY }, { panelX + kRestraintPanelWidth, panelY });
    restraintPanelBorders_[1]->SetPoints({ panelX, panelY + kRestraintPanelHeight }, { panelX + kRestraintPanelWidth, panelY + kRestraintPanelHeight });
    restraintPanelBorders_[2]->SetPoints({ panelX, panelY }, { panelX, panelY + kRestraintPanelHeight });
    restraintPanelBorders_[3]->SetPoints({ panelX + kRestraintPanelWidth, panelY }, { panelX + kRestraintPanelWidth, panelY + kRestraintPanelHeight });
    for (int i = 0; i < 4; ++i) {
        restraintPanelBorders_[i]->GetMaterialData()->color = borderColor;
        restraintPanelBorders_[i]->Update();
        restraintPanelBorders_[i]->Draw("white", true);
    }

    // 3. 警告タイトルテキスト（ラッシュ連動時は「! LOCKED - ENEMY RUSH 周囲警戒 !」へ切替）
    TextTextureGenerator* activeTitleGen = isRestraintRushActive_
        ? restraintRushTitleTextGen_.get()
        : restraintTitleTextGen_.get();
    float activeTitleW = isRestraintRushActive_ ? cachedRestraintRushTitleWidth_ : cachedRestraintTitleWidth_;
    float activeTitleH = isRestraintRushActive_ ? cachedRestraintRushTitleHeight_ : cachedRestraintTitleHeight_;
    if (restraintTitleTextObj_ && activeTitleGen && activeTitleW > 0.0f) {
        float titleX = panelX + (kRestraintPanelWidth - activeTitleW) * kHalf;
        float titleY = panelY + kTitleTopOffset;
        Vector4 titleColor = kDxRestraintTitleColor;
        titleColor.w = pulse;
        restraintTitleTextObj_->SetSize(activeTitleW, activeTitleH);
        restraintTitleTextObj_->SetPosition({ titleX, titleY, 0.0f });
        restraintTitleTextObj_->SetColor(titleColor);
        restraintTitleTextObj_->Update();
        restraintTitleTextObj_->Draw(activeTitleGen->GetGpuHandle(), true);
    }

    // 4. 残り時間カウントダウンテキスト（表示文字列変化時のみテクスチャ再生成）
    if (restraintTimerTextObj_ && restraintTimerTextGen_) {
        float remainSec = GetRestraintRemainingSeconds();
        char timerBuf[64];
        snprintf(timerBuf, sizeof(timerBuf), "UNLOCK IN %.1f s", remainSec);
        std::string timerStr(timerBuf);

        if (timerStr != lastRenderedRestraintTimerText_) {
            if (restraintTimerTextGen_->Generate(timerStr, "Yu Gothic UI", kRestraintTimerFontSize, 0, true, false, false)) {
                cachedRestraintTimerWidth_ = static_cast<float>(restraintTimerTextGen_->GetWidth());
                cachedRestraintTimerHeight_ = static_cast<float>(restraintTimerTextGen_->GetHeight());
                restraintTimerTextObj_->SetSize(cachedRestraintTimerWidth_, cachedRestraintTimerHeight_);
                lastRenderedRestraintTimerText_ = timerStr;
            }
        }
        if (cachedRestraintTimerWidth_ > 0.0f) {
            float timerX = panelX + (kRestraintPanelWidth - cachedRestraintTimerWidth_) * kHalf;
            float timerY = panelY + kTimerTopOffset;
            restraintTimerTextObj_->SetPosition({ timerX, timerY, 0.0f });
            restraintTimerTextObj_->SetColor(kDxRestraintTimerColor);
            restraintTimerTextObj_->Update();
            restraintTimerTextObj_->Draw(restraintTimerTextGen_->GetGpuHandle(), true);
        }
    }

    // 5. 解除カウントダウン・プログレスバー
    float barX = panelX + kRestraintBarPadding;
    float barY = panelY + kRestraintPanelHeight - kBarBottomMargin - kRestraintBarHeight;
    float barFullWidth = kRestraintPanelWidth - kRestraintBarPadding * 2.0f;

    if (restraintBarBgSprite_) {
        restraintBarBgSprite_->SetPosition({ barX, barY, 0.0f });
        restraintBarBgSprite_->SetSize(barFullWidth, kRestraintBarHeight);
        restraintBarBgSprite_->GetMaterialData()->color = kDxRestraintBarBgColor;
        restraintBarBgSprite_->Update();
        restraintBarBgSprite_->Draw("white", true);
    }

    float currentBarWidth = barFullWidth * remainRatio;
    if (restraintBarFillSprite_ && currentBarWidth > 0.0f) {
        restraintBarFillSprite_->SetPosition({ barX, barY, 0.0f });
        restraintBarFillSprite_->SetSize(currentBarWidth, kRestraintBarHeight);
        restraintBarFillSprite_->GetMaterialData()->color = kDxRestraintBarFillColor;
        restraintBarFillSprite_->Update();
        restraintBarFillSprite_->Draw("white", true);
    }
}

/// <summary>
/// Step 1-6: 霊魂吸収時・強化獲得時の上昇フェードHUDポップアップをトリガーします。
/// </summary>
void GamePhaseControllerScript::TriggerHudPopup(const std::string& text, const Vector4& color) {
    hudPopupText_ = text;
    hudPopupColor_ = color;
    hudPopupTimer_ = kHudPopupDurationFrames;
}

/// <summary>
/// Step 1-6: レベルアップ3択アップグレード画面を開き、戦闘一時停止・レティクル非表示・ランダム3択選出を行います。
/// </summary>
void GamePhaseControllerScript::OpenLevelUpChoice() {
    if (!playerScript_) return;

    isLevelUpChoosing_ = true;
    levelUpInputGuardTimer_ = kLevelUpInputGuardFrames;
    hoveredCardIndex_ = -1;
    levelUpAnimTimer_ = 0.0f;
    currentUpgradeChoices_.clear();

    auto choices = playerScript_->RollUpgradeChoices();
    for (auto type : choices) {
        currentUpgradeChoices_.push_back(static_cast<int>(type));
    }

    // 選択中は照準レティクルを隠してカード選択に集中させる
    SetReticleVisible(false);

    // レベルアップ通知音（軽快なシグナルSE）
    if (auto* audio = Audio::GetInstance()) {
        if (isSeLoaded_) {
            constexpr float kLevelUpSeVolume = 0.72f;
            audio->PlaySoundW(seStartSignal_, kLevelUpSeVolume, false);
        }
    }
}

/// <summary>
/// Step 1-6: 3択カード選択中のマウスホバー・左クリックおよび 1/2/3 キー入力を処理します。
/// </summary>
void GamePhaseControllerScript::UpdateLevelUpChoice() {
    constexpr float kAnimStep = 1.0f / 60.0f;
    levelUpAnimTimer_ += kAnimStep;

    if (levelUpInputGuardTimer_ > 0) {
        levelUpInputGuardTimer_--;
    }

    auto* input = InputResource::GetInput();
    if (!input || currentUpgradeChoices_.empty()) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    float targetMouseX = -1.0f;
    float targetMouseY = -1.0f;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        targetMouseX = mousePos.x * (kRenderWidth / viewSize.x);
        targetMouseY = mousePos.y * (kRenderHeight / viewSize.y);
    }

    int cardCount = static_cast<int>(currentUpgradeChoices_.size());
    float totalRowWidth = static_cast<float>(cardCount) * kChoiceCardWidth +
                          static_cast<float>((std::max)(0, cardCount - 1)) * kChoiceCardGap;
    float startX = (kRenderWidth - totalRowWidth) * 0.5f;

    hoveredCardIndex_ = -1;
    for (int i = 0; i < cardCount; ++i) {
        float cardX = startX + static_cast<float>(i) * (kChoiceCardWidth + kChoiceCardGap);
        float cardY = kChoiceCardBaseY;
        if (targetMouseX >= cardX && targetMouseX <= cardX + kChoiceCardWidth &&
            targetMouseY >= cardY - kChoiceCardHoverLiftY && targetMouseY <= cardY + kChoiceCardHeight) {
            hoveredCardIndex_ = i;
            break;
        }
    }

    // 誤クリック防止ガード期間中は決定入力を受け付けない
    if (levelUpInputGuardTimer_ > 0) {
        return;
    }

    // キーボード 1 / 2 / 3 キーによる即時選択
    constexpr BYTE kChoiceKeys[kLevelUpCardCount] = { DIK_1, DIK_2, DIK_3 };
    for (int i = 0; i < cardCount && i < kLevelUpCardCount; ++i) {
        if (input->Trigger(kChoiceKeys[i])) {
            ConfirmLevelUpChoice(i);
            return;
        }
    }

    // マウス左クリックによるホバー中カードの決定
    constexpr int kLeftMouseButton = 0;
    if (hoveredCardIndex_ >= 0 && input->MouseTrigger(kLeftMouseButton)) {
        ConfirmLevelUpChoice(hoveredCardIndex_);
    }
}

/// <summary>
/// Step 1-6: 選ばれた強化カードを自機へ適用し、祝福フラッシュ＆ポップアップを表示して戦闘へ復帰します。
/// </summary>
void GamePhaseControllerScript::ConfirmLevelUpChoice(int choiceIndex) {
    if (!playerScript_) return;
    if (choiceIndex < 0 || choiceIndex >= static_cast<int>(currentUpgradeChoices_.size())) return;

    auto selectedType = static_cast<PlayerControllerScript::UpgradeType>(currentUpgradeChoices_[choiceIndex]);
    playerScript_->ApplyUpgradeChoice(selectedType);

    auto info = playerScript_->GetUpgradeCardInfo(selectedType);
    char popBuf[128];
    snprintf(popBuf, sizeof(popBuf), "POWER UP!!  %s Lv.%d", info.title.c_str(), info.currentLevel);
    TriggerHudPopup(popBuf, info.themeColor);

    constexpr float kConfirmFlashAlpha = 0.28f;
    flashAlpha_ = kConfirmFlashAlpha;

    // 決定SE
    if (auto* audio = Audio::GetInstance()) {
        if (isSeLoaded_) {
            constexpr float kConfirmSeVol = 0.70f;
            audio->PlaySoundW(seCountdownBeep_, kConfirmSeVol, false);
        }
    }

    // 複数レベル同時上昇が残っている場合は続けて次の3択を展開
    if (playerScript_->HasPendingLevelUp()) {
        OpenLevelUpChoice();
        return;
    }

    isLevelUpChoosing_ = false;
    hoveredCardIndex_ = -1;
    currentUpgradeChoices_.clear();
    SetReticleVisible(true);
}

/// <summary>
/// Step 1-6: 全8種の強化に対応するDX12幾何学ベクターアイコンを任意の中心座標・サイズで描画します。
/// 画面上部の所持バフアイコンバーと3択強化カードの双方で共通利用します。
/// </summary>
void GamePhaseControllerScript::DrawUpgradeIconDirectX(
    int upgradeTypeIdx, float centerX, float centerY, float iconSize, const Vector4& themeColor) {

    auto drawLine = [&](const Vector2& a, const Vector2& b, float thickness, const Vector4& col) {
        if (iconLineCursor_ >= iconLinePool_.size()) return;
        auto& ln = iconLinePool_[iconLineCursor_++];
        ln->SetPoints(a, b);
        ln->SetThickness(thickness);
        ln->GetMaterialData()->color = col;
        ln->Update();
        ln->Draw("white", true);
    };
    auto drawCircle = [&](float cx, float cy, float r, const Vector4& col) {
        if (iconCircleCursor_ >= iconCirclePool_.size()) return;
        auto& cr = iconCirclePool_[iconCircleCursor_++];
        cr->SetRadius(r);
        cr->SetPosition({ cx - r, cy - r, 0.0f });
        cr->GetMaterialData()->color = col;
        cr->Update();
        cr->Draw("white", true);
    };
    auto drawRing = [&](float cx, float cy, float outerR, float innerR, const Vector4& col) {
        if (iconRingCursor_ >= iconRingPool_.size()) return;
        auto& rg = iconRingPool_[iconRingCursor_++];
        rg->SetRadii(outerR, (std::max)(1.0f, innerR));
        rg->SetPosition({ cx - outerR, cy - outerR, 0.0f });
        rg->GetMaterialData()->color = col;
        rg->Update();
        rg->Draw("white", true);
    };
    auto drawRect = [&](float x, float y, float w, float h, const Vector4& col) {
        if (iconSpriteCursor_ >= iconSpritePool_.size()) return;
        auto& sp = iconSpritePool_[iconSpriteCursor_++];
        sp->SetPosition({ x, y, 0.0f });
        sp->SetSize(w, h);
        sp->GetMaterialData()->color = col;
        sp->Update();
        sp->Draw("white", true);
    };

    float r = iconSize * 0.44f;
    float thick = (std::max)(1.8f, iconSize * 0.085f);
    constexpr Vector4 kHighlightWhite = { 1.0f, 1.0f, 1.0f, 0.95f };

    auto type = static_cast<PlayerControllerScript::UpgradeType>(upgradeTypeIdx);
    switch (type) {
    case PlayerControllerScript::UpgradeType::MultiShot:
        // 3方向扇状トリプルレーザー (╲ │ ╱) ＋ 基点コア
        drawLine({ centerX, centerY + r * 0.72f }, { centerX, centerY - r * 0.82f }, thick, kHighlightWhite);
        drawLine({ centerX, centerY + r * 0.72f }, { centerX - r * 0.78f, centerY - r * 0.58f }, thick, themeColor);
        drawLine({ centerX, centerY + r * 0.72f }, { centerX + r * 0.78f, centerY - r * 0.58f }, thick, themeColor);
        drawCircle(centerX, centerY + r * 0.72f, r * 0.22f, kHighlightWhite);
        break;

    case PlayerControllerScript::UpgradeType::PiercingLaser:
        // 標的リングを左→右へ貫く長大レーザー矢 (─◎→)
        drawRing(centerX, centerY, r * 0.62f, r * 0.42f, themeColor);
        drawLine({ centerX - r * 0.92f, centerY }, { centerX + r * 0.92f, centerY }, thick * 1.1f, kHighlightWhite);
        drawLine({ centerX + r * 0.48f, centerY - r * 0.36f }, { centerX + r * 0.92f, centerY }, thick, themeColor);
        drawLine({ centerX + r * 0.48f, centerY + r * 0.36f }, { centerX + r * 0.92f, centerY }, thick, themeColor);
        break;

    case PlayerControllerScript::UpgradeType::HighPower:
        // 上向きダブルシェブロン火力マーク (▲▲)
        drawLine({ centerX - r * 0.72f, centerY + r * 0.08f }, { centerX, centerY - r * 0.68f }, thick * 1.15f, kHighlightWhite);
        drawLine({ centerX + r * 0.72f, centerY + r * 0.08f }, { centerX, centerY - r * 0.68f }, thick * 1.15f, kHighlightWhite);
        drawLine({ centerX - r * 0.72f, centerY + r * 0.68f }, { centerX, centerY - r * 0.08f }, thick * 1.15f, themeColor);
        drawLine({ centerX + r * 0.72f, centerY + r * 0.68f }, { centerX, centerY - r * 0.08f }, thick * 1.15f, themeColor);
        break;

    case PlayerControllerScript::UpgradeType::RapidFire:
        // 斜め右上へ疾走する3連スピードレーザー条線
        drawLine({ centerX - r * 0.72f, centerY + r * 0.15f }, { centerX + r * 0.32f, centerY - r * 0.72f }, thick, themeColor);
        drawLine({ centerX - r * 0.46f, centerY + r * 0.48f }, { centerX + r * 0.62f, centerY - r * 0.42f }, thick * 1.1f, kHighlightWhite);
        drawLine({ centerX - r * 0.18f, centerY + r * 0.78f }, { centerX + r * 0.80f, centerY - r * 0.06f }, thick, themeColor);
        break;

    case PlayerControllerScript::UpgradeType::ExtendedMag: {
        // 3連弾倉カートリッジバー (▮▮▮) ＋ 右上プラス
        float barW = r * 0.34f;
        float barH = r * 1.25f;
        float gap = r * 0.54f;
        drawRect(centerX - gap - barW * 0.5f, centerY - barH * 0.42f, barW, barH, themeColor);
        drawRect(centerX - barW * 0.5f, centerY - barH * 0.52f, barW, barH, kHighlightWhite);
        drawRect(centerX + gap - barW * 0.5f, centerY - barH * 0.42f, barW, barH, themeColor);
        break;
    }

    case PlayerControllerScript::UpgradeType::QuickReload:
        // 回転リロードリング＋矢印ウィング (↻)
        drawRing(centerX, centerY, r * 0.74f, r * 0.48f, themeColor);
        drawLine({ centerX + r * 0.25f, centerY - r * 0.76f }, { centerX + r * 0.80f, centerY - r * 0.40f }, thick, kHighlightWhite);
        drawLine({ centerX + r * 0.80f, centerY - r * 0.40f }, { centerX + r * 0.36f, centerY - r * 0.14f }, thick, kHighlightWhite);
        break;

    case PlayerControllerScript::UpgradeType::ArmorRepair: {
        // くっきり太いメディカル十字クロス (✚)
        float armW = r * 0.46f;
        float armL = r * 1.48f;
        drawRect(centerX - armW * 0.5f, centerY - armL * 0.5f, armW, armL, themeColor);
        drawRect(centerX - armL * 0.5f, centerY - armW * 0.5f, armL, armW, themeColor);
        drawRect(centerX - armW * 0.24f, centerY - armL * 0.38f, armW * 0.48f, armL * 0.76f, kHighlightWhite);
        drawRect(centerX - armL * 0.38f, centerY - armW * 0.24f, armL * 0.76f, armW * 0.48f, kHighlightWhite);
        break;
    }

    case PlayerControllerScript::UpgradeType::SoulVacuumBoost:
        // 二重吸引リング＋中心霊魂コア＋4方向内向き吸引線 (◎)
        drawRing(centerX, centerY, r * 0.82f, r * 0.64f, themeColor);
        drawRing(centerX, centerY, r * 0.46f, r * 0.32f, kHighlightWhite);
        drawCircle(centerX, centerY, r * 0.20f, kHighlightWhite);
        drawLine({ centerX - r * 0.92f, centerY }, { centerX - r * 0.46f, centerY }, thick * 0.8f, themeColor);
        drawLine({ centerX + r * 0.92f, centerY }, { centerX + r * 0.46f, centerY }, thick * 0.8f, themeColor);
        drawLine({ centerX, centerY - r * 0.92f }, { centerX, centerY - r * 0.46f }, thick * 0.8f, themeColor);
        drawLine({ centerX, centerY + r * 0.92f }, { centerX, centerY + r * 0.46f }, thick * 0.8f, themeColor);
        break;

    default:
        drawCircle(centerX, centerY, r * 0.5f, themeColor);
        break;
    }
}

/// <summary>
/// Step 1-6: 画面最上部フルワイドEXPバー、画面上部左のLVバッジ＆所持バフアイコンバー、
/// および画面上部右の霊魂＆火力ステータス要約を描画します。
/// </summary>
void GamePhaseControllerScript::RenderExpAndBuffBarDirectX() {
    if (!playerScript_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kHalf = 0.5f;
    constexpr Vector4 kExpTrackColor = { 10.0f / 255.0f, 16.0f / 255.0f, 26.0f / 255.0f, 0.90f };
    constexpr Vector4 kExpFillCyan   = { 0.22f, 0.92f, 1.00f, 0.96f };
    constexpr Vector4 kExpFillGold   = { 1.00f, 0.85f, 0.22f, 1.00f };
    constexpr Vector4 kLvGoldBorder  = { 1.00f, 0.82f, 0.24f, 0.95f };
    constexpr Vector4 kSlotEmptyBg   = { 12.0f / 255.0f, 16.0f / 255.0f, 24.0f / 255.0f, 0.50f };
    constexpr Vector4 kSlotEmptyEdge = { 55.0f / 255.0f, 75.0f / 255.0f, 100.0f / 255.0f, 0.42f };
    constexpr Vector4 kSlotOwnedBg   = { 14.0f / 255.0f, 20.0f / 255.0f, 34.0f / 255.0f, 0.90f };

    int curExp = playerScript_->GetCurrentExp();
    int nextExp = playerScript_->GetExpToNextLevel();
    float expRatio = (nextExp > 0) ? std::clamp(static_cast<float>(curExp) / static_cast<float>(nextExp), 0.0f, 1.0f) : 0.0f;
    if (isLevelUpChoosing_) {
        expRatio = 1.0f;
    }

    // 1. 画面最上部 フルワイド EXP ゲージバー (1280 x 10px)
    if (expBarBgSprite_) {
        expBarBgSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        expBarBgSprite_->SetSize(kRenderWidth, kTopExpBarHeight);
        expBarBgSprite_->GetMaterialData()->color = kExpTrackColor;
        expBarBgSprite_->Update();
        expBarBgSprite_->Draw("white", true);
    }
    float fillW = kRenderWidth * expRatio;
    if (expBarFillSprite_ && fillW > 1.0f) {
        Vector4 barColor = (expRatio >= 0.85f || isLevelUpChoosing_) ? kExpFillGold : kExpFillCyan;
        expBarFillSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        expBarFillSprite_->SetSize(fillW, kTopExpBarHeight);
        expBarFillSprite_->GetMaterialData()->color = barColor;
        expBarFillSprite_->Update();
        expBarFillSprite_->Draw("white", true);
    }
    if (expBarBottomLine_) {
        expBarBottomLine_->SetPoints({ 0.0f, kTopExpBarHeight }, { kRenderWidth, kTopExpBarHeight });
        expBarBottomLine_->GetMaterialData()->color = { 0.25f, 0.82f, 1.0f, 0.55f };
        expBarBottomLine_->Update();
        expBarBottomLine_->Draw("white", true);
    }

    // 2. 画面上部・左: LVバッジ (X = 20..86, Y = 18..68)
    if (lvBadgeBgSprite_) {
        lvBadgeBgSprite_->SetPosition({ kLvBadgeX, kLvBadgeY, 0.0f });
        lvBadgeBgSprite_->SetSize(kLvBadgeWidth, kLvBadgeHeight);
        lvBadgeBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
        lvBadgeBgSprite_->Update();
        lvBadgeBgSprite_->Draw("white", true);
    }
    lvBadgeBorders_[0]->SetPoints({ kLvBadgeX, kLvBadgeY }, { kLvBadgeX + kLvBadgeWidth, kLvBadgeY });
    lvBadgeBorders_[1]->SetPoints({ kLvBadgeX, kLvBadgeY + kLvBadgeHeight }, { kLvBadgeX + kLvBadgeWidth, kLvBadgeY + kLvBadgeHeight });
    lvBadgeBorders_[2]->SetPoints({ kLvBadgeX, kLvBadgeY }, { kLvBadgeX, kLvBadgeY + kLvBadgeHeight });
    lvBadgeBorders_[3]->SetPoints({ kLvBadgeX + kLvBadgeWidth, kLvBadgeY }, { kLvBadgeX + kLvBadgeWidth, kLvBadgeY + kLvBadgeHeight });
    for (int i = 0; i < 4; ++i) {
        lvBadgeBorders_[i]->GetMaterialData()->color = kLvGoldBorder;
        lvBadgeBorders_[i]->Update();
        lvBadgeBorders_[i]->Draw("white", true);
    }

    char lvBuf[32];
    snprintf(lvBuf, sizeof(lvBuf), "LV.%d", playerScript_->GetPlayerLevel());
    if (lvBadgeTextGen_ && lvBadgeTextObj_) {
        constexpr float kLvFontSize = 12.5f;
        lvBadgeTextGen_->Generate(lvBuf, "Yu Gothic UI", kLvFontSize, 1, true, false, false);
        float tw = static_cast<float>(lvBadgeTextGen_->GetWidth());
        float th = static_cast<float>(lvBadgeTextGen_->GetHeight());
        float textAreaH = kLvBadgeHeight - kLvBadgeGaugeHeight - kLvBadgeGaugeBottomPad - 2.0f;
        lvBadgeTextObj_->SetSize(tw, th);
        lvBadgeTextObj_->SetPosition({ kLvBadgeX + (kLvBadgeWidth - tw) * kHalf, kLvBadgeY + (textAreaH - th) * kHalf, 0.0f });
        lvBadgeTextObj_->SetColor(kExpFillGold);
        lvBadgeTextObj_->Update();
        lvBadgeTextObj_->Draw(lvBadgeTextGen_->GetGpuHandle(), true);
    }

    // LVバッジ下部: 数値テキストではなく純粋なEXPゲージバーで進捗表示（HPバーと統一デザイン）
    float miniBarX = kLvBadgeX + kLvBadgeGaugePadX;
    float miniBarY = kLvBadgeY + kLvBadgeHeight - kLvBadgeGaugeBottomPad - kLvBadgeGaugeHeight;
    float miniBarFullW = kLvBadgeWidth - kLvBadgeGaugePadX * 2.0f;
    if (lvExpBarBgSprite_) {
        lvExpBarBgSprite_->SetPosition({ miniBarX, miniBarY, 0.0f });
        lvExpBarBgSprite_->SetSize(miniBarFullW, kLvBadgeGaugeHeight);
        lvExpBarBgSprite_->GetMaterialData()->color = kExpTrackColor;
        lvExpBarBgSprite_->Update();
        lvExpBarBgSprite_->Draw("white", true);
    }
    float miniBarFillW = miniBarFullW * expRatio;
    if (lvExpBarFillSprite_ && miniBarFillW > 0.5f) {
        Vector4 miniFillColor = (expRatio >= 0.85f || isLevelUpChoosing_) ? kExpFillGold : kExpFillCyan;
        lvExpBarFillSprite_->SetPosition({ miniBarX, miniBarY, 0.0f });
        lvExpBarFillSprite_->SetSize(miniBarFillW, kLvBadgeGaugeHeight);
        lvExpBarFillSprite_->GetMaterialData()->color = miniFillColor;
        lvExpBarFillSprite_->Update();
        lvExpBarFillSprite_->Draw("white", true);
    }

    // 3. 画面上部・左: 所持バフアイコンバー（最大8スロット横並び・コンパスバーに一切干渉しない）
    const auto& ownedOrder = playerScript_->GetOwnedUpgradeOrder();
    int ownedCount = static_cast<int>(ownedOrder.size());
    constexpr int kMinVisibleSlots = 4; // 未取得時も装備スロット感が伝わるよう最低4枠を薄く表示
    int displaySlots = (std::min)(kMaxBuffIconSlots, (std::max)(kMinVisibleSlots, ownedCount));

    for (int s = 0; s < displaySlots; ++s) {
        float slotX = kBuffBarStartX + static_cast<float>(s) * (kBuffSlotWidth + kBuffSlotGap);
        float slotY = kLvBadgeY;
        bool hasBuff = (s < ownedCount);

        Vector4 bgCol = kSlotEmptyBg;
        Vector4 edgeCol = kSlotEmptyEdge;
        PlayerControllerScript::UpgradeCardInfo info{};

        if (hasBuff) {
            info = playerScript_->GetUpgradeCardInfo(ownedOrder[s]);
            bgCol = kSlotOwnedBg;
            edgeCol = info.themeColor;
        }

        if (buffSlotBgSprites_[s]) {
            buffSlotBgSprites_[s]->SetPosition({ slotX, slotY, 0.0f });
            buffSlotBgSprites_[s]->SetSize(kBuffSlotWidth, kBuffSlotHeight);
            buffSlotBgSprites_[s]->GetMaterialData()->color = bgCol;
            buffSlotBgSprites_[s]->Update();
            buffSlotBgSprites_[s]->Draw("white", true);
        }

        buffSlotBorders_[s][0]->SetPoints({ slotX, slotY }, { slotX + kBuffSlotWidth, slotY });
        buffSlotBorders_[s][1]->SetPoints({ slotX, slotY + kBuffSlotHeight }, { slotX + kBuffSlotWidth, slotY + kBuffSlotHeight });
        buffSlotBorders_[s][2]->SetPoints({ slotX, slotY }, { slotX, slotY + kBuffSlotHeight });
        buffSlotBorders_[s][3]->SetPoints({ slotX + kBuffSlotWidth, slotY }, { slotX + kBuffSlotWidth, slotY + kBuffSlotHeight });
        for (int b = 0; b < 4; ++b) {
            buffSlotBorders_[s][b]->GetMaterialData()->color = edgeCol;
            buffSlotBorders_[s][b]->Update();
            buffSlotBorders_[s][b]->Draw("white", true);
        }

        if (hasBuff) {
            // スロット上部中央に専用幾何学ベクターアイコンを描画
            constexpr float kSlotIconSize = 24.0f;
            float iconCenterX = slotX + kBuffSlotWidth * kHalf;
            float iconCenterY = slotY + 16.0f;
            DrawUpgradeIconDirectX(static_cast<int>(info.type), iconCenterX, iconCenterY, kSlotIconSize, info.themeColor);

            // スロット下部にレベル表記 (Lv1.. / MAX)
            char slotLvBuf[16];
            bool isMax = (info.currentLevel >= info.maxLevel);
            if (isMax) {
                snprintf(slotLvBuf, sizeof(slotLvBuf), "MAX");
            } else {
                snprintf(slotLvBuf, sizeof(slotLvBuf), "Lv%d", info.currentLevel);
            }
            if (buffSlotLvTextGens_[s] && buffSlotLvTextObjs_[s]) {
                constexpr float kSlotLvFontSize = 8.5f;
                buffSlotLvTextGens_[s]->Generate(slotLvBuf, "Consolas", kSlotLvFontSize, 1, true, false, false);
                float tw = static_cast<float>(buffSlotLvTextGens_[s]->GetWidth());
                float th = static_cast<float>(buffSlotLvTextGens_[s]->GetHeight());
                buffSlotLvTextObjs_[s]->SetSize(tw, th);
                buffSlotLvTextObjs_[s]->SetPosition({ slotX + (kBuffSlotWidth - tw) * kHalf, slotY + kBuffSlotHeight - th - 2.0f, 0.0f });
                buffSlotLvTextObjs_[s]->SetColor(isMax ? kExpFillGold : Vector4{ 0.94f, 0.98f, 1.0f, 1.0f });
                buffSlotLvTextObjs_[s]->Update();
                buffSlotLvTextObjs_[s]->Draw(buffSlotLvTextGens_[s]->GetGpuHandle(), true);
            }
        }
    }

    // 4. 画面上部・右: 霊魂カウント ＆ 武器スペック要約パネル
    float statsX = kRenderWidth - kStatsPanelMarginRight - kStatsPanelWidth;
    float statsY = kLvBadgeY;

    if (statsPanelBgSprite_) {
        statsPanelBgSprite_->SetPosition({ statsX, statsY, 0.0f });
        statsPanelBgSprite_->SetSize(kStatsPanelWidth, kStatsPanelHeight);
        statsPanelBgSprite_->GetMaterialData()->color = kDxPanelBgColor;
        statsPanelBgSprite_->Update();
        statsPanelBgSprite_->Draw("white", true);
    }
    statsPanelBorders_[0]->SetPoints({ statsX, statsY }, { statsX + kStatsPanelWidth, statsY });
    statsPanelBorders_[1]->SetPoints({ statsX, statsY + kStatsPanelHeight }, { statsX + kStatsPanelWidth, statsY + kStatsPanelHeight });
    statsPanelBorders_[2]->SetPoints({ statsX, statsY }, { statsX, statsY + kStatsPanelHeight });
    statsPanelBorders_[3]->SetPoints({ statsX + kStatsPanelWidth, statsY }, { statsX + kStatsPanelWidth, statsY + kStatsPanelHeight });
    for (int i = 0; i < 4; ++i) {
        statsPanelBorders_[i]->GetMaterialData()->color = kDxPanelBorderColor;
        statsPanelBorders_[i]->Update();
        statsPanelBorders_[i]->Draw("white", true);
    }

    char statsBuf[96];
    snprintf(statsBuf, sizeof(statsBuf), "SOULS:%d | DMG:%d  x%d  PRC:%d",
        playerScript_->GetAbsorbedGhostCount(),
        playerScript_->GetBulletDamage(),
        playerScript_->GetMultiShotCount(),
        playerScript_->GetBulletPierceCount());

    if (statsPanelTextGen_ && statsPanelTextObj_) {
        constexpr float kStatsFontSize = 12.0f;
        statsPanelTextGen_->Generate(statsBuf, "Consolas", kStatsFontSize, 1, true, false, false);
        float tw = static_cast<float>(statsPanelTextGen_->GetWidth());
        float th = static_cast<float>(statsPanelTextGen_->GetHeight());
        statsPanelTextObj_->SetSize(tw, th);
        statsPanelTextObj_->SetPosition({ statsX + (kStatsPanelWidth - tw) * kHalf, statsY + (kStatsPanelHeight - th) * kHalf, 0.0f });
        statsPanelTextObj_->SetColor(kDxTextColor);
        statsPanelTextObj_->Update();
        statsPanelTextObj_->Draw(statsPanelTextGen_->GetGpuHandle(), true);
    }
}

/// <summary>
/// Step 1-6: ダダサバイバー風の3択アップグレードカード選択画面（暗転オーバーレイ＋大型ベクターアイコン付き3枚カード）を描画します。
/// </summary>
void GamePhaseControllerScript::RenderLevelUpChoiceDirectX() {
    if (!playerScript_ || currentUpgradeChoices_.empty()) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);
    constexpr float kHalf = 0.5f;

    constexpr Vector4 kDimColor           = { 0.02f, 0.04f, 0.08f, 0.78f };
    constexpr Vector4 kHeaderBgColor      = { 0.06f, 0.09f, 0.16f, 0.94f };
    constexpr Vector4 kHeaderGoldColor    = { 1.00f, 0.85f, 0.24f, 1.00f };
    constexpr Vector4 kCardBgNormal       = { 0.06f, 0.08f, 0.14f, 0.94f };
    constexpr Vector4 kCardBgHover        = { 0.11f, 0.15f, 0.25f, 0.98f };
    constexpr Vector4 kIconBoxBg          = { 0.03f, 0.05f, 0.09f, 0.95f };
    constexpr Vector4 kDescTextColor      = { 0.84f, 0.92f, 1.00f, 1.00f };

    // 1. 全画面ダークオーバーレイ
    if (levelUpDimSprite_) {
        levelUpDimSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        levelUpDimSprite_->SetSize(kRenderWidth, kRenderHeight);
        levelUpDimSprite_->GetMaterialData()->color = kDimColor;
        levelUpDimSprite_->Update();
        levelUpDimSprite_->Draw("white", true);
    }

    // 2. 上部「★ LEVEL UP!! ★」見出しパネル
    float headerX = (kRenderWidth - kLevelUpHeaderWidth) * kHalf;
    float headerY = kLevelUpHeaderY;
    if (levelUpHeaderBgSprite_) {
        levelUpHeaderBgSprite_->SetPosition({ headerX, headerY, 0.0f });
        levelUpHeaderBgSprite_->SetSize(kLevelUpHeaderWidth, kLevelUpHeaderHeight);
        levelUpHeaderBgSprite_->GetMaterialData()->color = kHeaderBgColor;
        levelUpHeaderBgSprite_->Update();
        levelUpHeaderBgSprite_->Draw("white", true);
    }
    levelUpHeaderBorders_[0]->SetPoints({ headerX, headerY }, { headerX + kLevelUpHeaderWidth, headerY });
    levelUpHeaderBorders_[1]->SetPoints({ headerX, headerY + kLevelUpHeaderHeight }, { headerX + kLevelUpHeaderWidth, headerY + kLevelUpHeaderHeight });
    levelUpHeaderBorders_[2]->SetPoints({ headerX, headerY }, { headerX, headerY + kLevelUpHeaderHeight });
    levelUpHeaderBorders_[3]->SetPoints({ headerX + kLevelUpHeaderWidth, headerY }, { headerX + kLevelUpHeaderWidth, headerY + kLevelUpHeaderHeight });
    for (int i = 0; i < 4; ++i) {
        levelUpHeaderBorders_[i]->GetMaterialData()->color = kHeaderGoldColor;
        levelUpHeaderBorders_[i]->Update();
        levelUpHeaderBorders_[i]->Draw("white", true);
    }

    char headBuf[64];
    snprintf(headBuf, sizeof(headBuf), "★ LEVEL UP!!  LV.%d ★", playerScript_->GetPlayerLevel());
    float headerCursorY = headerY + 6.0f;
    if (levelUpHeaderTextGen_ && levelUpHeaderTextObj_) {
        constexpr float kHeadFontSize = 20.0f;
        levelUpHeaderTextGen_->Generate(headBuf, "Yu Gothic UI", kHeadFontSize, 1, true, false, false);
        float tw = static_cast<float>(levelUpHeaderTextGen_->GetWidth());
        float th = static_cast<float>(levelUpHeaderTextGen_->GetHeight());
        levelUpHeaderTextObj_->SetSize(tw, th);
        levelUpHeaderTextObj_->SetPosition({ headerX + (kLevelUpHeaderWidth - tw) * kHalf, headerCursorY, 0.0f });
        levelUpHeaderTextObj_->SetColor(kHeaderGoldColor);
        levelUpHeaderTextObj_->Update();
        levelUpHeaderTextObj_->Draw(levelUpHeaderTextGen_->GetGpuHandle(), true);
        headerCursorY += th + 2.0f;
    }

    if (levelUpSubTextGen_ && levelUpSubTextObj_) {
        constexpr float kSubFontSize = 11.5f;
        levelUpSubTextGen_->Generate("強化を1つ選択してください  [ クリック または 1 / 2 / 3 キー ]", "Yu Gothic UI", kSubFontSize, 1, true, false, false);
        float tw = static_cast<float>(levelUpSubTextGen_->GetWidth());
        float th = static_cast<float>(levelUpSubTextGen_->GetHeight());
        levelUpSubTextObj_->SetSize(tw, th);
        levelUpSubTextObj_->SetPosition({ headerX + (kLevelUpHeaderWidth - tw) * kHalf, headerCursorY, 0.0f });
        levelUpSubTextObj_->SetColor(kDescTextColor);
        levelUpSubTextObj_->Update();
        levelUpSubTextObj_->Draw(levelUpSubTextGen_->GetGpuHandle(), true);
    }

    // 3. 画面中央の3択強化カード横並び描画
    int cardCount = static_cast<int>(currentUpgradeChoices_.size());
    float totalRowWidth = static_cast<float>(cardCount) * kChoiceCardWidth +
                          static_cast<float>((std::max)(0, cardCount - 1)) * kChoiceCardGap;
    float startX = (kRenderWidth - totalRowWidth) * kHalf;

    for (int i = 0; i < cardCount && i < kLevelUpCardCount; ++i) {
        auto type = static_cast<PlayerControllerScript::UpgradeType>(currentUpgradeChoices_[i]);
        auto info = playerScript_->GetUpgradeCardInfo(type);
        bool isHovered = (i == hoveredCardIndex_);

        float cardX = startX + static_cast<float>(i) * (kChoiceCardWidth + kChoiceCardGap);
        float cardY = kChoiceCardBaseY - (isHovered ? kChoiceCardHoverLiftY : 0.0f);

        // カード背景
        if (choiceCardBgSprites_[i]) {
            choiceCardBgSprites_[i]->SetPosition({ cardX, cardY, 0.0f });
            choiceCardBgSprites_[i]->SetSize(kChoiceCardWidth, kChoiceCardHeight);
            choiceCardBgSprites_[i]->GetMaterialData()->color = isHovered ? kCardBgHover : kCardBgNormal;
            choiceCardBgSprites_[i]->Update();
            choiceCardBgSprites_[i]->Draw("white", true);
        }

        // 上部カテゴリ色帯
        if (choiceCardTopBandSprites_[i]) {
            Vector4 bandCol = info.themeColor;
            bandCol.w = isHovered ? 0.95f : 0.76f;
            choiceCardTopBandSprites_[i]->SetPosition({ cardX, cardY, 0.0f });
            choiceCardTopBandSprites_[i]->SetSize(kChoiceCardWidth, kChoiceCardBandHeight);
            choiceCardTopBandSprites_[i]->GetMaterialData()->color = bandCol;
            choiceCardTopBandSprites_[i]->Update();
            choiceCardTopBandSprites_[i]->Draw("white", true);
        }

        // カテゴリ名テキスト
        if (choiceCardCatTextGens_[i] && choiceCardCatTextObjs_[i]) {
            constexpr float kCatFontSize = 13.0f;
            choiceCardCatTextGens_[i]->Generate(info.categoryLabel, "Yu Gothic UI", kCatFontSize, 1, true, false, false);
            float tw = static_cast<float>(choiceCardCatTextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardCatTextGens_[i]->GetHeight());
            choiceCardCatTextObjs_[i]->SetSize(tw, th);
            choiceCardCatTextObjs_[i]->SetPosition({ cardX + (kChoiceCardWidth - tw) * kHalf, cardY + (kChoiceCardBandHeight - th) * kHalf, 0.0f });
            choiceCardCatTextObjs_[i]->SetColor({ 0.04f, 0.06f, 0.10f, 1.0f });
            choiceCardCatTextObjs_[i]->Update();
            choiceCardCatTextObjs_[i]->Draw(choiceCardCatTextGens_[i]->GetGpuHandle(), true);
        }

        // カード外枠（ホバー時は純白に近い発光強調）
        Vector4 cardBorderCol = isHovered ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : info.themeColor;
        choiceCardBorders_[i][0]->SetPoints({ cardX, cardY }, { cardX + kChoiceCardWidth, cardY });
        choiceCardBorders_[i][1]->SetPoints({ cardX, cardY + kChoiceCardHeight }, { cardX + kChoiceCardWidth, cardY + kChoiceCardHeight });
        choiceCardBorders_[i][2]->SetPoints({ cardX, cardY }, { cardX, cardY + kChoiceCardHeight });
        choiceCardBorders_[i][3]->SetPoints({ cardX + kChoiceCardWidth, cardY }, { cardX + kChoiceCardWidth, cardY + kChoiceCardHeight });
        for (int b = 0; b < 4; ++b) {
            choiceCardBorders_[i][b]->SetThickness(isHovered ? 3.2f : 2.0f);
            choiceCardBorders_[i][b]->GetMaterialData()->color = cardBorderCol;
            choiceCardBorders_[i][b]->Update();
            choiceCardBorders_[i][b]->Draw("white", true);
        }

        // カード中央上部の大型ベクターアイコンボックス
        float iconBoxX = cardX + (kChoiceCardWidth - kChoiceCardIconBoxSize) * kHalf;
        float iconBoxY = cardY + kChoiceCardBandHeight + 22.0f;
        if (choiceCardIconBoxSprites_[i]) {
            choiceCardIconBoxSprites_[i]->SetPosition({ iconBoxX, iconBoxY, 0.0f });
            choiceCardIconBoxSprites_[i]->SetSize(kChoiceCardIconBoxSize, kChoiceCardIconBoxSize);
            choiceCardIconBoxSprites_[i]->GetMaterialData()->color = kIconBoxBg;
            choiceCardIconBoxSprites_[i]->Update();
            choiceCardIconBoxSprites_[i]->Draw("white", true);
        }
        choiceCardIconBoxBorders_[i][0]->SetPoints({ iconBoxX, iconBoxY }, { iconBoxX + kChoiceCardIconBoxSize, iconBoxY });
        choiceCardIconBoxBorders_[i][1]->SetPoints({ iconBoxX, iconBoxY + kChoiceCardIconBoxSize }, { iconBoxX + kChoiceCardIconBoxSize, iconBoxY + kChoiceCardIconBoxSize });
        choiceCardIconBoxBorders_[i][2]->SetPoints({ iconBoxX, iconBoxY }, { iconBoxX, iconBoxY + kChoiceCardIconBoxSize });
        choiceCardIconBoxBorders_[i][3]->SetPoints({ iconBoxX + kChoiceCardIconBoxSize, iconBoxY }, { iconBoxX + kChoiceCardIconBoxSize, iconBoxY + kChoiceCardIconBoxSize });
        for (int b = 0; b < 4; ++b) {
            choiceCardIconBoxBorders_[i][b]->GetMaterialData()->color = info.themeColor;
            choiceCardIconBoxBorders_[i][b]->Update();
            choiceCardIconBoxBorders_[i][b]->Draw("white", true);
        }

        // 上部バフバーと共通デザインの大型ベクターアイコンを描画
        constexpr float kLargeIconSize = 66.0f;
        DrawUpgradeIconDirectX(
            static_cast<int>(info.type),
            iconBoxX + kChoiceCardIconBoxSize * kHalf,
            iconBoxY + kChoiceCardIconBoxSize * kHalf,
            kLargeIconSize,
            info.themeColor);

        // カード内テキストはすべて生成テクスチャ実測高さ(GetHeight)に基づく動的縦積み配置で重なりを物理的にゼロ化
        float cursorY = iconBoxY + kChoiceCardIconBoxSize + 14.0f;

        // スキル名
        if (choiceCardTitleTextGens_[i] && choiceCardTitleTextObjs_[i]) {
            constexpr float kTitleFontSize = 17.0f;
            choiceCardTitleTextGens_[i]->Generate(info.title, "Yu Gothic UI", kTitleFontSize, 1, true, false, false);
            float tw = static_cast<float>(choiceCardTitleTextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardTitleTextGens_[i]->GetHeight());
            choiceCardTitleTextObjs_[i]->SetSize(tw, th);
            choiceCardTitleTextObjs_[i]->SetPosition({ cardX + (kChoiceCardWidth - tw) * kHalf, cursorY, 0.0f });
            choiceCardTitleTextObjs_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            choiceCardTitleTextObjs_[i]->Update();
            choiceCardTitleTextObjs_[i]->Draw(choiceCardTitleTextGens_[i]->GetGpuHandle(), true);
            cursorY += th + 4.0f;
        }

        // レベル遷移バッジ (NEW! / Lv.1 → Lv.2)
        char cardLvBuf[48];
        if (info.currentLevel == 0) {
            snprintf(cardLvBuf, sizeof(cardLvBuf), "NEW!  (Lv.1 / %d)", info.maxLevel);
        } else if (info.currentLevel < info.maxLevel) {
            snprintf(cardLvBuf, sizeof(cardLvBuf), "Lv.%d  ->  Lv.%d", info.currentLevel, info.currentLevel + 1);
        } else {
            snprintf(cardLvBuf, sizeof(cardLvBuf), "BONUS! (MAX強化)");
        }
        if (choiceCardLvTextGens_[i] && choiceCardLvTextObjs_[i]) {
            constexpr float kLvFontSize = 12.5f;
            choiceCardLvTextGens_[i]->Generate(cardLvBuf, "Consolas", kLvFontSize, 1, true, false, false);
            float tw = static_cast<float>(choiceCardLvTextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardLvTextGens_[i]->GetHeight());
            choiceCardLvTextObjs_[i]->SetSize(tw, th);
            choiceCardLvTextObjs_[i]->SetPosition({ cardX + (kChoiceCardWidth - tw) * kHalf, cursorY, 0.0f });
            choiceCardLvTextObjs_[i]->SetColor(info.themeColor);
            choiceCardLvTextObjs_[i]->Update();
            choiceCardLvTextObjs_[i]->Draw(choiceCardLvTextGens_[i]->GetGpuHandle(), true);
            cursorY += th + 12.0f;
        }

        // 効果説明（改行で2行分割表示・実測高さ積み上げ）
        std::string descLine1 = info.description;
        std::string descLine2;
        size_t nlPos = info.description.find('\n');
        if (nlPos != std::string::npos) {
            descLine1 = info.description.substr(0, nlPos);
            descLine2 = info.description.substr(nlPos + 1);
        }

        constexpr float kDescFontSize = 12.5f;
        if (choiceCardDesc1TextGens_[i] && choiceCardDesc1TextObjs_[i] && !descLine1.empty()) {
            choiceCardDesc1TextGens_[i]->Generate(descLine1, "Yu Gothic UI", kDescFontSize, 1, true, false, false);
            float tw = static_cast<float>(choiceCardDesc1TextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardDesc1TextGens_[i]->GetHeight());
            choiceCardDesc1TextObjs_[i]->SetSize(tw, th);
            choiceCardDesc1TextObjs_[i]->SetPosition({ cardX + (kChoiceCardWidth - tw) * kHalf, cursorY, 0.0f });
            choiceCardDesc1TextObjs_[i]->SetColor(kDescTextColor);
            choiceCardDesc1TextObjs_[i]->Update();
            choiceCardDesc1TextObjs_[i]->Draw(choiceCardDesc1TextGens_[i]->GetGpuHandle(), true);
            cursorY += th + 4.0f;
        }
        if (choiceCardDesc2TextGens_[i] && choiceCardDesc2TextObjs_[i] && !descLine2.empty()) {
            choiceCardDesc2TextGens_[i]->Generate(descLine2, "Yu Gothic UI", kDescFontSize, 1, false, false, false);
            float tw = static_cast<float>(choiceCardDesc2TextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardDesc2TextGens_[i]->GetHeight());
            choiceCardDesc2TextObjs_[i]->SetSize(tw, th);
            choiceCardDesc2TextObjs_[i]->SetPosition({ cardX + (kChoiceCardWidth - tw) * kHalf, cursorY, 0.0f });
            choiceCardDesc2TextObjs_[i]->SetColor(kDxSubTextColor);
            choiceCardDesc2TextObjs_[i]->Update();
            choiceCardDesc2TextObjs_[i]->Draw(choiceCardDesc2TextGens_[i]->GetGpuHandle(), true);
        }

        // 下部選択ボタン ([ 1 ] 選択)
        constexpr float kBtnMargin = 18.0f;
        float btnX = cardX + kBtnMargin;
        float btnW = kChoiceCardWidth - kBtnMargin * 2.0f;
        float btnY = cardY + kChoiceCardHeight - kChoiceCardBtnHeight - 16.0f;
        if (choiceCardBtnSprites_[i]) {
            Vector4 btnCol = info.themeColor;
            btnCol.w = isHovered ? 0.96f : 0.35f;
            choiceCardBtnSprites_[i]->SetPosition({ btnX, btnY, 0.0f });
            choiceCardBtnSprites_[i]->SetSize(btnW, kChoiceCardBtnHeight);
            choiceCardBtnSprites_[i]->GetMaterialData()->color = btnCol;
            choiceCardBtnSprites_[i]->Update();
            choiceCardBtnSprites_[i]->Draw("white", true);
        }

        char keyBuf[32];
        snprintf(keyBuf, sizeof(keyBuf), "[ %d ] この強化を選択", i + 1);
        if (choiceCardKeyTextGens_[i] && choiceCardKeyTextObjs_[i]) {
            constexpr float kBtnFontSize = 14.0f;
            choiceCardKeyTextGens_[i]->Generate(keyBuf, "Yu Gothic UI", kBtnFontSize, 1, true, false, false);
            float tw = static_cast<float>(choiceCardKeyTextGens_[i]->GetWidth());
            float th = static_cast<float>(choiceCardKeyTextGens_[i]->GetHeight());
            choiceCardKeyTextObjs_[i]->SetSize(tw, th);
            choiceCardKeyTextObjs_[i]->SetPosition({ btnX + (btnW - tw) * kHalf, btnY + (kChoiceCardBtnHeight - th) * kHalf, 0.0f });
            choiceCardKeyTextObjs_[i]->SetColor(isHovered ? Vector4{ 0.04f, 0.06f, 0.10f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 0.95f });
            choiceCardKeyTextObjs_[i]->Update();
            choiceCardKeyTextObjs_[i]->Draw(choiceCardKeyTextGens_[i]->GetGpuHandle(), true);
        }
    }
}

/// <summary>
/// Step 1-6: 霊魂吸収・強化完了時の爽快な上昇フェードHUDポップアップを描画します。
/// </summary>
void GamePhaseControllerScript::RenderHudPopupDirectX() {
    if (hudPopupTimer_ <= 0 || hudPopupText_.empty()) return;
    if (!hudPopupTextGen_ || !hudPopupTextObj_ || !hudPopupBgSprite_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kHalf = 0.5f;
    constexpr float kPopupFontSize = 18.0f;
    constexpr float kPopupPadX = 24.0f;
    constexpr float kPopupPadY = 8.0f;

    float progress = 1.0f - static_cast<float>(hudPopupTimer_) / static_cast<float>(kHudPopupDurationFrames);
    float fadeAlpha = (progress > 0.65f) ? std::clamp((1.0f - progress) / 0.35f, 0.0f, 1.0f) : 1.0f;
    float popupY = kHudPopupBaseY - progress * kHudPopupRiseDist;

    hudPopupTextGen_->Generate(hudPopupText_, "Yu Gothic UI", kPopupFontSize, 1, true, false, false);
    float tw = static_cast<float>(hudPopupTextGen_->GetWidth());
    float th = static_cast<float>(hudPopupTextGen_->GetHeight());

    float bgW = tw + kPopupPadX * 2.0f;
    float bgH = th + kPopupPadY * 2.0f;
    float bgX = (kRenderWidth - bgW) * kHalf;

    hudPopupBgSprite_->SetPosition({ bgX, popupY, 0.0f });
    hudPopupBgSprite_->SetSize(bgW, bgH);
    hudPopupBgSprite_->GetMaterialData()->color = { 0.04f, 0.07f, 0.12f, 0.78f * fadeAlpha };
    hudPopupBgSprite_->Update();
    hudPopupBgSprite_->Draw("white", true);

    Vector4 textCol = hudPopupColor_;
    textCol.w *= fadeAlpha;
    hudPopupTextObj_->SetSize(tw, th);
    hudPopupTextObj_->SetPosition({ bgX + kPopupPadX, popupY + kPopupPadY, 0.0f });
    hudPopupTextObj_->SetColor(textCol);
    hudPopupTextObj_->Update();
    hudPopupTextObj_->Draw(hudPopupTextGen_->GetGpuHandle(), true);
}



