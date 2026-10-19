#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Route/RouteManagerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Enemy/EnemySpawnManagerScript.h"
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

    // パルス演出定数
    constexpr float kGoalPulseSpeed = 2.5f;
    constexpr float kGoalPulseSpread = 10.0f;
    constexpr float kGoalPulseMaxAlpha = 220.0f;

    // レイアウト定数
    constexpr float kHeaderHeight = 36.0f;
    constexpr float kRouteLineThickness = 2.5f;
    constexpr float kCompletedLineThickness = 3.0f;
    constexpr float kMapBoundaryX = 50.0f; // ルート境界半幅
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

    // ゲームオーバー判定（自機死亡）
    if (playerScript_ && playerScript_->IsDead()) {
        SceneManager::GetInstance()->ChangeScene(kGameOverSceneName);
        return;
    }

    // フェーズごとの処理
    if (currentPhase_ == Phase::DrawRoute) {
        UpdateDrawRoutePhase();
    } else if (currentPhase_ == Phase::Play) {
        UpdatePlayPhase();
    }
}

void GamePhaseControllerScript::UpdateDrawRoutePhase() {
    auto* input = InputResource::GetInput();
    if (!input || !routeMgr_) return;

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) return;

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

    // GOボタンまたはSPACEキーで走行開始（ゴール到達時のみ）
    if (routeMgr_->HasReachedGoal()) {
        if (input->Trigger(DIK_SPACE) || (isTrigger && isMouseOverGoBtn)) {
            TransitionToPlay();
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

                if (std::abs(rayDir.y) > 0.0001f) {
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

void GamePhaseControllerScript::UpdatePlayPhase() {
    if (!routeMgr_ || !playerScript_) return;

    currentDistance_ += playerMoveSpeed_;

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

void GamePhaseControllerScript::HandleAreaClear() {
    // 前エリアの残敵・残留弾を一括消去
    ClearAllEnemies();

    currentArea_++;
    if (currentArea_ >= kMaxAreas) {
        // 全エリアクリア
        SceneManager::GetInstance()->ChangeScene(kClearSceneName);
    } else {
        // 次のエリアへ移行
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
}

void GamePhaseControllerScript::TransitionToDraw() {
    currentPhase_ = Phase::DrawRoute;

    // 前エリアの敵・弾を完全消去し、照準レティクルを非表示化
    ClearAllEnemies();
    SetReticleVisible(false);

    // カメラを斜め立体俯瞰位置へ移動（奥行きと3D立体ラインが綺麗に見渡せる画角）
    auto* cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr) {
        BaseCamera* camera = cameraMgr->GetActiveCamera();
        if (camera && routeMgr_) {
            float startZ = routeMgr_->GetCurrentAreaStartZ();
            camera->SetPosition({ 0.0f, kDrawCameraHeight, startZ + kDrawCameraZOffset });
            camera->SetRotation({ kDrawCameraPitch, 0.0f, 0.0f });
        }
    }

    if (playerScript_) {
        playerScript_->SetAutoMoving(false);
    }
}

void GamePhaseControllerScript::TransitionToPlay() {
    currentPhase_ = Phase::Play;
    currentDistance_ = 0.0f;

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

    ImGui::TextColored(kSubHeaderColor, "【ルート走行設定】");
    ImGui::DragFloat("ルート走行速度##PlayerMoveSpeed", &playerMoveSpeed_, 0.005f, kMinPlayerMoveSpeed, kMaxPlayerMoveSpeed, "%.3f");
    if (ImGui::Button("速度リセット (0.15)##ResetSpeed", ImVec2(kResetBtnWidth, kResetBtnHeight))) {
        playerMoveSpeed_ = kDefaultPlayerMoveSpeed;
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

    // 1. 2Dミニマップの描画 (DirectX 12)
    Render2DMapDirectX();

    // 2. ゴール到達時のGOサイン・リセットボタンの描画 (DirectX 12)
    if (currentPhase_ == Phase::DrawRoute && routeMgr_ && routeMgr_->HasReachedGoal()) {
        RenderGoSignDirectX();
    }
}

void GamePhaseControllerScript::OnGUI() {
    // Release構成・Debug構成共通で DirectX 12 描画パイプライン（Draw2D）に完全移行済み
}

void GamePhaseControllerScript::DrawHUD() {
}

void GamePhaseControllerScript::Draw2DMap(const Vector2&, const Vector2&) {
}

void GamePhaseControllerScript::DrawGoSign(const Vector2&, const Vector2&) {
}

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

    // 9. スタート地点・ゴール地点の描画
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


