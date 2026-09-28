#include "App/Scene/Game/Phase/DrawRoutePhase.h"
#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "App/Scene/Game/Route.h"
#include "App/Scene/Game/Stage/Stage.h"
#include "App/Scene/Game/UI/Minimap.h"
#include "App/Scene/Game/Camera/DrawRouteCamera.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Zuizui.h"
#include "externals/imgui/imgui.h"
#include "App/Scene/Game/Stage/StageEditor.h"

/**
 * @brief コンストラクタ
 * @param input キー入力・マウス入力のマネージャ
 * @param cameraMgr カメラマネージャ
 * @param route ルートデータクラス
 * @param stage ステージ障害物情報
 * @param minimap 左画面のミニマップUI
 * @param drawRouteCamera 手書きモード用のズームカメラ
 * @param cursorIndicatorZoom 右3D画面用の床面カーソル指示球体
 * @param stageEditor エネミー配置用のステージエディタ
 * @param onStartGame ゴール到達後にゲーム（走行プレイ）を開始するためのコールバック関数
 */
DrawRoutePhase::DrawRoutePhase(
    Input* input,
    CameraManager* cameraMgr,
    Route* route,
    Stage* stage,
    Minimap* minimap,
    DrawRouteCamera* drawRouteCamera,
    SphereObject* cursorIndicatorZoom,
    StageEditor* stageEditor,
    std::function<void()> onStartGame
) : input_(input),
    cameraMgr_(cameraMgr),
    route_(route),
    stage_(stage),
    minimap_(minimap),
    drawRouteCamera_(drawRouteCamera),
    cursorIndicatorZoom_(cursorIndicatorZoom),
    stageEditor_(stageEditor),
    onStartGame_(onStartGame) {}

void DrawRoutePhase::Initialize() {
    // 描画フェーズ開始時にアクティブカメラをZoomに設定（遷移時1回のみ）
    cameraMgr_->SetActiveCamera("Zoom");
}

void DrawRoutePhase::Update() {
    auto dxCommon = Zuizui::GetInstance()->GetDxCommon();
    float clientW = dxCommon ? dxCommon->GetViewport().Width : static_cast<float>(WindowApp::kClientWidth);
    float clientH = dxCommon ? dxCommon->GetViewport().Height : static_cast<float>(WindowApp::kClientHeight);

    constexpr float kSplitRightRatio = 0.7f;

    // 1. 2Dミニマップの更新
    Vector3 targetZoom = drawRouteCamera_->GetDestinationZoom();
    minimap_->Update(input_, route_, stage_, targetZoom, stageEditor_);
    drawRouteCamera_->SetDestinationZoom(targetZoom);

    // 2. ズームカメラの更新
    drawRouteCamera_->Update(input_, route_->GetCurrentAreaStartZ(), route_->GetCurrentAreaGoalZ());

    // 3. 右画面（Zoomカメラ 3D空間用）のWVP計算・更新
    drawRouteCamera_->GetCamera()->UpdateProjection((clientW * kSplitRightRatio) / clientH); // 右画面用アスペクト比を設定
    stage_->Update();

    // 3D赤丸インジケータ（右画面の床用）の更新
    Vector3 indicatorPos = { drawRouteCamera_->GetTargetZoom().x, 0.1f, drawRouteCamera_->GetTargetZoom().z };
    cursorIndicatorZoom_->SetPosition(indicatorPos);
    cursorIndicatorZoom_->Update();

    // Rキーで手書きルートをクリア（引き直し）する
    if (input_->Trigger(DIK_R)) {
        route_->ClearForNewArea();
    }

    // 4. ゴールまで到達している場合、SPACEキーでゲーム開始できるようにする
    if (route_->HasReachedGoal() && input_->Trigger(DIK_SPACE)) {
        if (onStartGame_) {
            onStartGame_();
            return;
        }
    }

    // 5. ステージエディタの更新（描画フェーズ中のみ有効）
    if (stageEditor_) {
        constexpr float kSplitLeftRatio = 0.3f;
        stageEditor_->SetViewportRatio(kSplitLeftRatio, kSplitRightRatio);
        stageEditor_->Update();
    }
}

void DrawRoutePhase::Draw() {
    // 画面分割（スプリットビュー）の描画
    auto dxCommon = Zuizui::GetInstance()->GetDxCommon();
    float clientW = dxCommon ? dxCommon->GetViewport().Width : static_cast<float>(WindowApp::kClientWidth);
    float clientH = dxCommon ? dxCommon->GetViewport().Height : static_cast<float>(WindowApp::kClientHeight);

    constexpr float kSplitLeftRatio = 0.3f;
    constexpr float kSplitRightRatio = 0.7f;

    // ビューポート・シザーの定義
    // 左側 (30%)
    D3D12_VIEWPORT vpLeft{};
    vpLeft.Width = clientW * kSplitLeftRatio;
    vpLeft.Height = clientH;
    vpLeft.TopLeftX = 0.0f;
    vpLeft.TopLeftY = 0.0f;
    vpLeft.MinDepth = 0.0f;
    vpLeft.MaxDepth = 1.0f;

    D3D12_RECT scLeft{};
    scLeft.left = 0;
    scLeft.right = static_cast<LONG>(vpLeft.Width);
    scLeft.top = 0;
    scLeft.bottom = static_cast<LONG>(clientH);

    // 右側 (70%)
    D3D12_VIEWPORT vpRight{};
    vpRight.Width = clientW * kSplitRightRatio;
    vpRight.Height = clientH;
    vpRight.TopLeftX = clientW * kSplitLeftRatio;
    vpRight.TopLeftY = 0.0f;
    vpRight.MinDepth = 0.0f;
    vpRight.MaxDepth = 1.0f;

    D3D12_RECT scRight{};
    scRight.left = static_cast<LONG>(vpRight.TopLeftX);
    scRight.right = static_cast<LONG>(clientW);
    scRight.top = 0;
    scRight.bottom = static_cast<LONG>(clientH);

    auto commandList = dxCommon->GetCommandList();

    // 1. 左画面の描画（2Dミニマップ）
    commandList->RSSetViewports(1, &vpLeft);
    commandList->RSSetScissorRects(1, &scLeft);
    minimap_->Draw(route_->GetCurrentAreaIndex());

    // 2. 右画面の描画（Zoomカメラ 3Dビュー）
    drawRouteCamera_->GetCamera()->UpdateProjection(vpRight.Width / vpRight.Height);

    commandList->RSSetViewports(1, &vpRight);
    commandList->RSSetScissorRects(1, &scRight);

    // Zoomカメラビューポートでの3D描画
    static const Vector3 dummyPlayerPos = { 0.0f, 0.0f, -240.0f };
    stage_->Draw(true, false, dummyPlayerPos);
    route_->Draw();
    cursorIndicatorZoom_->Draw();

    // エディタで配置された敵サークルの描画
    if (stageEditor_) {
        stageEditor_->Draw3D();
    }
}

void DrawRoutePhase::ImGuiControl() {
#ifdef _USEIMGUI
    if (!showRouteEditor_ || *showRouteEditor_) {
        if (ImGui::Begin("ルートエディタ###Route Editor", showRouteEditor_)) {
            ImGui::Text("マウスドラッグで地面にルートを描画");
            ImGui::Text("ゴール到達: %s", route_->HasReachedGoal() ? "到達済み" : "未到達");
            
            if (route_->HasReachedGoal()) {
                if (ImGui::Button("ゲーム開始")) {
                    if (onStartGame_) {
                        ImGui::End();
                        onStartGame_();
                        if (stageEditor_) {
                            stageEditor_->ImGuiControl();
                        }
                        return;
                    }
                }
            } else {
                ImGui::TextDisabled("黄色のスタート球から青色のゴール球までドラッグしてください。");
            }
        }
        ImGui::End();
    }

    if (stageEditor_) {
        stageEditor_->ImGuiControl();
    }
#endif
}
