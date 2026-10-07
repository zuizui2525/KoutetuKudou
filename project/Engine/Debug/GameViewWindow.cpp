#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <windows.h>
#include <algorithm>

#ifdef _USEIMGUI
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "App/Scene/Core/SceneManager.h"
#include "externals/imgui/imgui.h"
#include "Engine/Debug/DebugEditor.h"

namespace {
    // ゲーム状態に応じたGameView枠線（ボーダー）の定数 (マジックナンバー排除)
    static constexpr float kGameViewBorderThickness = 4.0f;
    static constexpr ImU32 kPausedBorderColor = IM_COL32(235, 55, 55, 255);       // 一時停止中: 鮮明な赤
    static constexpr ImU32 kRunningBorderColor = IM_COL32(45, 140, 245, 255);     // 再生中: 爽やかな青
}

GameViewWindow::GameViewWindow() = default;

void GameViewWindow::Draw(bool* show, bool* isVisible) {
    *isVisible = false;
    sIsGameViewVisible_ = false;
    sIsGameViewFocused_ = false;
    
    if (ImGui::Begin("ゲーム画面###Game View", show)) {
        *isVisible = true;
        sIsGameViewVisible_ = true;
        sIsGameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // ゲーム画面描画エリア（内部のみWindowPaddingを0にする）
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("GameRenderArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        PostProcess* postProcess = SceneManager::GetInstance()->GetPostProcess();
        if (postProcess) {
            // 子ウィンドウのサイズを取得し、アスペクト比を維持したサイズを計算
            ImVec2 contentSize = ImGui::GetContentRegionAvail();
            
            // ゲーム画面の本来のアスペクト比（16:9）を定数定義（マジックナンバー排除）
            constexpr float kGameAspectWidth = 16.0f;
            constexpr float kGameAspectHeight = 9.0f;
            constexpr float kGameAspectRatio = kGameAspectWidth / kGameAspectHeight;
            
            float width = contentSize.x;
            float height = contentSize.x / kGameAspectRatio;
            
            if (height > contentSize.y) {
                height = contentSize.y;
                width = contentSize.y * kGameAspectRatio;
            }
            
            // 中央揃え用のパディング計算
            ImVec2 cursorPadding = ImVec2(
                (contentSize.x - width) * 0.5f,
                (contentSize.y - height) * 0.5f
            );
            ImGui::SetCursorPos(cursorPadding);
            
            // ポストプロセスの最終結果テクスチャを描画
            D3D12_GPU_DESCRIPTOR_HANDLE finalSrv = postProcess->GetFinalSrvGpuHandle();
            ImTextureID texID = (ImTextureID)finalSrv.ptr;
            
            ImVec2 imgPosMin = ImGui::GetCursorScreenPos();
            ImGui::Image(texID, ImVec2(width, height));

            // マウスがゲーム描画画像上にあるか判定（ドラッグ中も判定を維持）
            sIsMouseOnGameView_ = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

            // ゲーム描画領域のサイズとスクリーン座標（左上）を保存
            sGameViewSize_ = { width, height };
            sGameViewPosMin_ = { imgPosMin.x, imgPosMin.y };

            // 現在の再生／一時停止状態を取得
            bool isPaused = false;
            if (auto debugEditor = Zuizui::GetInstance()->GetDebugEditor()) {
                isPaused = debugEditor->IsPaused();
            }

            // 外枠の色（ボーダー）を描画して、再生中（青）か停止中（赤）かを一目で判別可能にする
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImU32 borderColor = isPaused ? kPausedBorderColor : kRunningBorderColor;
            ImVec2 imgPosMax = ImVec2(imgPosMin.x + width, imgPosMin.y + height);
            drawList->AddRect(imgPosMin, imgPosMax, borderColor, 0.0f, 0, kGameViewBorderThickness);

            // 中央座標の計算
            ImVec2 center = ImVec2(imgPosMin.x + width * 0.5f, imgPosMin.y + height * 0.5f);

            // 再生/一時停止ポップアニメーションの更新と描画
            popAnim_.Update(ImGui::GetIO().DeltaTime);
            popAnim_.Draw(ImGui::GetWindowDrawList(), center);

        } else {
            ImGui::Text("No Active PostProcess");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    } else {
        sIsMouseOnGameView_ = false;
    }
    ImGui::End();
}
#endif

// -------------------------------------------------------------
// 静的関数の実装（_USEIMGUIの定義に関わらず利用可能）
// -------------------------------------------------------------

bool GameViewWindow::IsMouseOnGameView() {
#ifdef _USEIMGUI
    return sIsMouseOnGameView_;
#else
    return true; // ImGuiが無ければ画面全体がゲーム画面なので常にtrue
#endif
}

Vector2 GameViewWindow::GetGameViewSize() {
#ifdef _USEIMGUI
    return sGameViewSize_;
#else
    // クライアント領域のサイズを取得
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();
    RECT clientRect{};
    if (GetClientRect(hwnd, &clientRect)) {
        return Vector2{ static_cast<float>(clientRect.right - clientRect.left), static_cast<float>(clientRect.bottom - clientRect.top) };
    }
    return Vector2{ static_cast<float>(WindowApp::kClientWidth), static_cast<float>(WindowApp::kClientHeight) };
#endif
}

Vector2 GameViewWindow::GetMousePosition() {
#ifdef _USEIMGUI
    ImVec2 mousePos = ImGui::GetMousePos();
    Vector2 localPos = Vector2{ mousePos.x - sGameViewPosMin_.x, mousePos.y - sGameViewPosMin_.y };
    if (sGameViewSize_.x > 0.0f && sGameViewSize_.y > 0.0f) {
        localPos.x = std::clamp(localPos.x, 0.0f, sGameViewSize_.x);
        localPos.y = std::clamp(localPos.y, 0.0f, sGameViewSize_.y);
    }
    return localPos;
#else
    // ウィンドウのクライアント領域上のマウス座標を取得
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();
    POINT point;
    if (GetCursorPos(&point) && ScreenToClient(hwnd, &point)) {
        Vector2 localPos = Vector2{ static_cast<float>(point.x), static_cast<float>(point.y) };
        Vector2 viewSize = GetGameViewSize();
        if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
            localPos.x = std::clamp(localPos.x, 0.0f, viewSize.x);
            localPos.y = std::clamp(localPos.y, 0.0f, viewSize.y);
        }
        return localPos;
    }
    return Vector2{ 0.0f, 0.0f };
#endif
}

Vector2 GameViewWindow::GetGameViewPosMin() {
#ifdef _USEIMGUI
    return sGameViewPosMin_;
#else
    return Vector2{ 0.0f, 0.0f };
#endif
}

bool GameViewWindow::WorldToScreen(const Vector3& worldPos, const BaseCamera* camera, Vector2& outScreenPos, float vpOffsetXRatio, float vpWidthRatio) {
    if (!camera) return false;

    // View行列およびProjection行列の乗算 (行優先: View * Projection)
    Matrix4x4 viewMat = camera->GetViewMatrix();
    Matrix4x4 projMat = camera->GetProjectionMatrix();
    Matrix4x4 viewProj = Math::Multiply(viewMat, projMat);

    // 行ベクトル * 行列 によるクリップ座標の計算
    float w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    static constexpr float kNearClipW = 0.001f;
    if (w <= kNearClipW) {
        return false; // カメラの背後またはクリップ面手前の場合は除外
    }

    float invW = 1.0f / w;
    float ndcX = (worldPos.x * viewProj.m[0][0] + worldPos.y * viewProj.m[1][0] + worldPos.z * viewProj.m[2][0] + viewProj.m[3][0]) * invW;
    float ndcY = (worldPos.x * viewProj.m[0][1] + worldPos.y * viewProj.m[1][1] + worldPos.z * viewProj.m[2][1] + viewProj.m[3][1]) * invW;

    static constexpr float kHalfCoord = 0.5f;
    Vector2 viewPos = GetGameViewPosMin();
    Vector2 viewSize = GetGameViewSize();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        return false;
    }

    // NDC [-1, 1] をスクリーンピクセル座標へ変換 (DirectX系のY軸反転を考慮、ビューポートオフセット・幅比率を適用)
    float localScreenX = (vpOffsetXRatio + (ndcX * kHalfCoord + kHalfCoord) * vpWidthRatio) * viewSize.x;
    outScreenPos.x = viewPos.x + localScreenX;
    outScreenPos.y = viewPos.y + (-ndcY * kHalfCoord + kHalfCoord) * viewSize.y;
    return true;
}
