#include "Engine/Debug/SceneViewWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <windows.h>

#ifdef _USEIMGUI
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "App/Scene/Core/SceneManager.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/ImGuizmo.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/IGameObject.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Triangle/Triangle2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Input/Input.h"
#include "Engine/Debug/DebugEditor.h"
#include "Engine/Debug/Command/CommandHistory.h"
#include "Engine/Debug/Command/TransformCommand.h"
#include "Engine/Debug/Command/LinePointsCommand.h"
#include "App/Scene/Game/Stage/StageEditor.h"
#include <algorithm>

namespace {
    // オブジェクトのTransform（位置・回転・拡縮）を取得する共通ヘルパー
    bool GetObjectTransform(IGameObject* obj, Transform& outTr) {
        if (!obj) return false;
        if (auto* go = dynamic_cast<GameObject*>(obj)) {
            outTr.translate = go->GetPosition();
            outTr.rotate = go->GetRotate();
            outTr.scale = go->GetScale();
            return true;
        } else if (auto* obj3d = dynamic_cast<Object3D*>(obj)) {
            outTr.translate = obj3d->GetPosition();
            outTr.rotate = obj3d->GetRotate();
            outTr.scale = obj3d->GetScale();
            return true;
        } else if (auto* sprite = dynamic_cast<SpriteObject*>(obj)) {
            outTr.translate = sprite->GetPosition();
            outTr.rotate = sprite->GetRotate();
            outTr.scale = sprite->GetScale();
            return true;
        } else if (auto* tri = dynamic_cast<Triangle2DObject*>(obj)) {
            outTr.translate = tri->GetPosition();
            outTr.rotate = tri->GetRotate();
            outTr.scale = tri->GetScale();
            return true;
        } else if (auto* circle = dynamic_cast<Circle2DObject*>(obj)) {
            outTr.translate = circle->GetPosition();
            outTr.rotate = circle->GetRotate();
            outTr.scale = circle->GetScale();
            return true;
        } else if (auto* ring = dynamic_cast<Ring2DObject*>(obj)) {
            outTr.translate = ring->GetPosition();
            outTr.rotate = ring->GetRotate();
            outTr.scale = ring->GetScale();
            return true;
        } else if (auto* cam = dynamic_cast<BaseCamera*>(obj)) {
            outTr.translate = cam->GetPosition();
            outTr.rotate = cam->GetRotation();
            outTr.scale = { 1.0f, 1.0f, 1.0f };
            return true;
        } else if (auto* light = dynamic_cast<DirectionalLightObject*>(obj)) {
            outTr.translate = light->GetPosition();
            outTr.rotate = light->GetRotate();
            outTr.scale = { 1.0f, 1.0f, 1.0f };
            return true;
        }
        return false;
    }

    // ゲーム状態に応じたGameView枠線・ステータス表示の定数 (マジックナンバー排除)
    static constexpr float kGameViewBorderThickness = 4.0f;
    static constexpr ImU32 kPausedBorderColor = IM_COL32(235, 55, 55, 255);       // 一時停止中: 鮮明な赤
    static constexpr ImU32 kRunningBorderColor = IM_COL32(45, 140, 245, 255);     // 再生中: 爽やかな青
    static constexpr ImU32 kPausedBannerBgColor = IM_COL32(200, 40, 40, 230);     // 一時停止中ステータス帯
    static constexpr ImU32 kRunningBannerBgColor = IM_COL32(30, 95, 190, 230);     // 再生中ステータス帯
    static constexpr ImU32 kStatusTextColor = IM_COL32(255, 255, 255, 255);       // ステータステキスト色
    static constexpr float kBadgePaddingX = 8.0f;
    static constexpr float kBadgePaddingY = 4.0f;
    static constexpr float kBadgeMargin = 8.0f;
    static constexpr float kBadgeRounding = 4.0f;

    // カメラ情報バッジ用の定数 (マジックナンバー排除)
    static constexpr float kCamBadgeMargin = 8.0f;
    static constexpr float kCamBadgePaddingX = 10.0f;
    static constexpr float kCamBadgePaddingY = 4.0f;
    static constexpr float kCamBadgeRounding = 4.0f;
    static constexpr float kCamBadgeBorderThickness = 1.0f;
    static constexpr ImU32 kCamBadgeBgColor = IM_COL32(18, 26, 38, 220);         // 上品な半透明ダークネイビー
    static constexpr ImU32 kCamBadgeBorderColor = IM_COL32(60, 160, 220, 200);   // 控えめなシアン枠線
    static constexpr ImU32 kCamBadgeTextColor = IM_COL32(100, 220, 255, 255);    // 視認性の高いシアン文字

    // ImGuizmoのハッチング線厚さ定数（0.0fで矢印上の変な破線を完全無効化）
    static constexpr float kDisabledHatchedAxisThickness = 0.0f;

    // トランスフォームの変化検知しきい値 (マジックナンバー排除)
    static constexpr float kTransformChangeThreshold = 1e-4f;

    // ギズモ操作前後の Undo/Redo コマンド記録用の状態変数
    bool sWasUsingGizmo = false;
    std::string sBeforeTargetName = "";
    Transform sBeforeTransform{};
    bool sIsTrackingTransform = false;
    Vector2 sBeforeLineStart{};
    Vector2 sBeforeLineEnd{};
    bool sIsTrackingLine2D = false;

    bool RaySphereIntersection(const Vector3& rayOrigin, const Vector3& rayDir, const Vector3& sphereCenter, float sphereRadius, float& outT) {
        Vector3 m = Math::Subtract(rayOrigin, sphereCenter);
        float b = Math::Dot(m, rayDir);
        float c = Math::Dot(m, m) - sphereRadius * sphereRadius;

        if (c > 0.0f && b > 0.0f) {
            return false;
        }

        float discr = b * b - c;
        if (discr < 0.0f) {
            return false;
        }

        float t = -b - sqrtf(discr);
        if (t < 0.0f) {
            t = 0.0f;
        }
        outT = t;
        return true;
    }
}

SceneViewWindow::SceneViewWindow()
    : showGizmo_(true) {
#ifdef _USEIMGUI
    ImGuizmo::GetStyle().HatchedAxisLineThickness = kDisabledHatchedAxisThickness;
#endif
}

void SceneViewWindow::Draw(bool* show) {
    sIsSceneViewVisible_ = false;
    sIsSceneViewFocused_ = false;
    if (ImGui::Begin("シーン (編集)###Scene View", show)) {
        sIsSceneViewVisible_ = true;
        sIsSceneViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
            sIsMouseOnSceneView_ = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

            // ゲーム描画領域のサイズとスクリーン座標（左上）を保存
            sSceneViewSize_ = { width, height };
            sSceneViewPosMin_ = { imgPosMin.x, imgPosMin.y };

            // ゲーム停止状態（ポーズ）に応じた枠線（ボーダー）およびステータスバッジの描画
            bool isPaused = false;
            if (auto debugEditor = Zuizui::GetInstance()->GetDebugEditor()) {
                isPaused = debugEditor->IsPaused();
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImU32 borderColor = isPaused ? kPausedBorderColor : kRunningBorderColor;
            ImVec2 imgPosMax = ImVec2(imgPosMin.x + width, imgPosMin.y + height);
            drawList->AddRect(imgPosMin, imgPosMax, borderColor, 0.0f, 0, kGameViewBorderThickness);

            // 画面左上に状態ステータスバッジ（赤: 一時停止中 / 青: 進行中）を描画
            const char* statusText = isPaused ? " [|| 一時停止中 - 敵配置可能] " : " [> 進行中 - ゲーム動作中] ";
            ImVec2 textSize = ImGui::CalcTextSize(statusText);
            ImVec2 badgeMin = ImVec2(imgPosMin.x + kBadgeMargin, imgPosMin.y + kBadgeMargin);
            ImVec2 badgeMax = ImVec2(badgeMin.x + textSize.x + kBadgePaddingX * 2.0f, badgeMin.y + textSize.y + kBadgePaddingY * 2.0f);
            ImU32 bannerColor = isPaused ? kPausedBannerBgColor : kRunningBannerBgColor;
            drawList->AddRectFilled(badgeMin, badgeMax, bannerColor, kBadgeRounding);
            drawList->AddText(ImVec2(badgeMin.x + kBadgePaddingX, badgeMin.y + kBadgePaddingY), kStatusTextColor, statusText);

            // 画面右上に現在レンダリングに使用しているアクティブカメラ名バッジを描画
            if (auto cameraMgr = CameraResource::GetCameraManager()) {
                std::string activeCamName = cameraMgr->GetActiveCameraName();
                if (activeCamName.empty()) {
                    activeCamName = "None";
                }
                std::string camBadgeText = " [Camera: " + activeCamName + "] ";
                ImVec2 camTextSize = ImGui::CalcTextSize(camBadgeText.c_str());
                float camBadgeWidth = camTextSize.x + kCamBadgePaddingX * 2.0f;
                float camBadgeHeight = camTextSize.y + kCamBadgePaddingY * 2.0f;
                ImVec2 camBadgeMin = ImVec2(imgPosMax.x - kCamBadgeMargin - camBadgeWidth, imgPosMin.y + kCamBadgeMargin);
                ImVec2 camBadgeMax = ImVec2(camBadgeMin.x + camBadgeWidth, camBadgeMin.y + camBadgeHeight);

                drawList->AddRectFilled(camBadgeMin, camBadgeMax, kCamBadgeBgColor, kCamBadgeRounding);
                drawList->AddRect(camBadgeMin, camBadgeMax, kCamBadgeBorderColor, kCamBadgeRounding, 0, kCamBadgeBorderThickness);
                drawList->AddText(ImVec2(camBadgeMin.x + kCamBadgePaddingX, camBadgeMin.y + kCamBadgePaddingY), kCamBadgeTextColor, camBadgeText.c_str());
            }

            // カメラモード切り替えバー（再生中・ポーズ中問わず常時利用可能）
            constexpr float kCamOverlayOffsetX = 8.0f;
            constexpr float kCamOverlayWidth = 230.0f;
            constexpr float kCamOverlayInnerPaddingX = 8.0f;
            constexpr ImU32 kCamOverlayBgColor = IM_COL32(20, 25, 35, 220); // バッジと同調する半透明ダーク背景

            ImVec2 camOverlayMin = ImVec2(badgeMax.x + kCamOverlayOffsetX, badgeMin.y);
            float badgeBarHeight = badgeMax.y - badgeMin.y;
            ImVec2 camOverlayMax = ImVec2(camOverlayMin.x + kCamOverlayWidth, badgeMin.y + badgeBarHeight);

            drawList->AddRectFilled(camOverlayMin, camOverlayMax, kCamOverlayBgColor, kBadgeRounding);

            ImVec2 currentMousePos = ImGui::GetMousePos();
            bool isCamOverlayHovered = (currentMousePos.x >= camOverlayMin.x && currentMousePos.x <= camOverlayMax.x &&
                                        currentMousePos.y >= camOverlayMin.y && currentMousePos.y <= camOverlayMax.y);

            // カメラ切替コントロール描画
            float camItemOffsetY = (badgeBarHeight - ImGui::GetFrameHeight()) * 0.5f;
            if (camItemOffsetY < 0.0f) camItemOffsetY = 0.0f;
            ImGui::SetCursorScreenPos(ImVec2(camOverlayMin.x + kCamOverlayInnerPaddingX, camOverlayMin.y + camItemOffsetY));

            ImGui::BeginGroup();
            bool isDebugMode = (sCameraMode_ == CameraMode::DebugCamera);
            if (ImGui::RadioButton("俯瞰", isDebugMode)) {
                sCameraMode_ = CameraMode::DebugCamera;
                if (auto camMgr = CameraResource::GetCameraManager()) {
                    camMgr->SetActiveCamera(CameraManager::kEditorCameraName);
                    if (auto* dc = dynamic_cast<DebugCamera*>(camMgr->GetActiveCamera())) {
                        dc->SetActive(true);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Main", !isDebugMode)) {
                sCameraMode_ = CameraMode::MainCamera;
                if (auto camMgr = CameraResource::GetCameraManager()) {
                    std::string mainCam = camMgr->GetDefaultGameCameraName();
                    if (!mainCam.empty()) {
                        camMgr->SetActiveCamera(mainCam);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("同期")) {
                if (auto camMgr = CameraResource::GetCameraManager()) {
                    std::string mainCamName = camMgr->GetDefaultGameCameraName();
                    BaseCamera* mainCam = camMgr->GetCamera(mainCamName);
                    if (auto* dc = dynamic_cast<DebugCamera*>(camMgr->GetCamera(CameraManager::kEditorCameraName))) {
                        if (mainCam) {
                            dc->SetPosition(mainCam->GetPosition());
                            dc->SetRotation(mainCam->GetCalculatedRotation());
                        }
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Debugカメラの位置・回転を現在のMainカメラと同期させます");
            }
            ImGui::EndGroup();

            // ゲーム停止中は常にギズモ操作を有効化（実行中はピッキング・ギズモ共に無効）
            bool isGizmoEffective = isPaused;
            bool isGizmoOverlayHovered = false;
            if (isGizmoEffective) {
                // オーバーレイUIの配置用定数（マジックナンバー排除）
                constexpr float kGizmoOverlayOffsetX = 8.0f;
                constexpr float kGizmoOverlayWidth = 230.0f;
                constexpr float kGizmoOverlayInnerPaddingX = 8.0f;
                constexpr ImU32 kGizmoOverlayBgColor = IM_COL32(20, 25, 35, 220); // バッジと同調する半透明ダーク背景

                ImVec2 overlayPos = ImVec2(camOverlayMax.x + kGizmoOverlayOffsetX, badgeMin.y);
                float overlayHeight = badgeMax.y - badgeMin.y;
                ImVec2 overlayMax = ImVec2(overlayPos.x + kGizmoOverlayWidth, overlayPos.y + overlayHeight);

                // 背景バッジを描画（ステータスバッジと同調）
                drawList->AddRectFilled(overlayPos, overlayMax, kGizmoOverlayBgColor, kBadgeRounding);

                // マウスがオーバーレイ上にあるか判定（レイキャスト誤クリック防止用）
                if (currentMousePos.x >= overlayPos.x && currentMousePos.x <= overlayMax.x &&
                    currentMousePos.y >= overlayPos.y && currentMousePos.y <= overlayMax.y) {
                    isGizmoOverlayHovered = true;
                }

                // カーソルをオーバーレイ内部へ配置（垂直中央揃えで確実に可視化）
                float itemOffsetY = (overlayHeight - ImGui::GetFrameHeight()) * 0.5f;
                if (itemOffsetY < 0.0f) {
                    itemOffsetY = 0.0f;
                }
                ImGui::SetCursorScreenPos(ImVec2(overlayPos.x + kGizmoOverlayInnerPaddingX, overlayPos.y + itemOffsetY));

                ImGui::BeginGroup();

                int currentOp = gizmoOperation_;
                bool opChanged = false;

                ImGui::Text("ギズモ:");
                ImGui::SameLine();
                if (ImGui::RadioButton("移動", currentOp == kGizmoOpTranslate)) {
                    currentOp = kGizmoOpTranslate;
                    opChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("回転", currentOp == kGizmoOpRotate)) {
                    currentOp = kGizmoOpRotate;
                    opChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("拡縮", currentOp == kGizmoOpScale)) {
                    currentOp = kGizmoOpScale;
                    opChanged = true;
                }

                if (opChanged) {
                    gizmoOperation_ = currentOp;
                }

                ImGui::EndGroup();
            }

            // ゲーム停止中（ポーズ中）のみピッキングを実行（ゲーム中の誤クリックを防止）
            BaseCamera* camera = CameraResource::GetCameraManager()->GetActiveCamera();
            bool isEditorPlacing = StageEditor::IsPlacingNow();
            if (isPaused && camera && sIsMouseOnSceneView_ && !isEditorPlacing) {
                // ギズモの操作子をホバー中・操作中、あるいはCtrlキー押下中、右クリック中、オーバーレイホバー中の場合はレイキャストを一切行わない（誤クリック防止）
                auto input = InputResource::GetInput();
                bool isCtrlPressed = input && (input->Press(DIK_LCONTROL) || input->Press(DIK_RCONTROL));
                bool isRightPressed = input && input->MousePress(1);
                bool isGizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing() || isCtrlPressed || isRightPressed || isGizmoOverlayHovered || isCamOverlayHovered;

                if (!isGizmoActive) {
                    bool isLeftClicked = ImGui::IsMouseClicked(0);

                    if (isLeftClicked) {
                        Vector2 relativeMousePos = GetMousePosition();
                        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();

                        // ----------------------------------------------------
                        // 1. 2Dオブジェクト優先ピッキング判定 (最前面のスクリーン座標)
                        // ----------------------------------------------------
                        float scaleX = static_cast<float>(WindowApp::kClientWidth) / width;
                        float scaleY = static_cast<float>(WindowApp::kClientHeight) / height;
                        Vector2 mouse2D = { relativeMousePos.x * scaleX, relativeMousePos.y * scaleY };

                        IGameObject* nearest2DObj = nullptr;
                        // 最前面優先（描画順の逆順）で探索
                        for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
                            IGameObject* obj = *it;
                            if (!obj || !obj->IsVisible()) continue;

                            // 1. コンポーネント指向 2D GameObject 判定（最優先）
                            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                                if (auto* sr = go->GetComponent<SpriteRendererComponent>()) {
                                    Vector3 pos = go->GetPosition();
                                    Vector3 scl = go->GetScale();
                                    auto shapeType = sr->GetShapeType();

                                    if (shapeType == SpriteRendererComponent::ShapeType::Sprite ||
                                        shapeType == SpriteRendererComponent::ShapeType::Triangle) {
                                        Vector2 sz = sr->GetSize();
                                        float w = sz.x * scl.x;
                                        float h = sz.y * scl.y;
                                        if (mouse2D.x >= pos.x && mouse2D.x <= pos.x + w &&
                                            mouse2D.y >= pos.y && mouse2D.y <= pos.y + h) {
                                            nearest2DObj = obj;
                                            break;
                                        }
                                    } else if (shapeType == SpriteRendererComponent::ShapeType::Circle) {
                                        float r = sr->GetRadius() * scl.x;
                                        float cx = pos.x + r;
                                        float cy = pos.y + r;
                                        float dx = mouse2D.x - cx;
                                        float dy = mouse2D.y - cy;
                                        if (dx * dx + dy * dy <= r * r) {
                                            nearest2DObj = obj;
                                            break;
                                        }
                                    } else if (shapeType == SpriteRendererComponent::ShapeType::Ring) {
                                        float outerR = sr->GetRadius() * scl.x;
                                        float cx = pos.x + outerR;
                                        float cy = pos.y + outerR;
                                        float dx = mouse2D.x - cx;
                                        float dy = mouse2D.y - cy;
                                        if (dx * dx + dy * dy <= outerR * outerR) {
                                            nearest2DObj = obj;
                                            break;
                                        }
                                    } else if (shapeType == SpriteRendererComponent::ShapeType::Line) {
                                        Vector2 start = { pos.x + sr->GetLineStart().x, pos.y + sr->GetLineStart().y };
                                        Vector2 end = { pos.x + sr->GetLineEnd().x, pos.y + sr->GetLineEnd().y };
                                        float thickness = sr->GetLineThickness();
                                        Vector2 ab = { end.x - start.x, end.y - start.y };
                                        Vector2 ap = { mouse2D.x - start.x, mouse2D.y - start.y };
                                        float abLenSq = ab.x * ab.x + ab.y * ab.y;
                                        constexpr float kMinLineLengthSq = 0.0001f;
                                        float t = (abLenSq > kMinLineLengthSq) ? (ap.x * ab.x + ap.y * ab.y) / abLenSq : 0.0f;
                                        t = (std::max)(0.0f, (std::min)(1.0f, t));
                                        Vector2 closest = { start.x + ab.x * t, start.y + ab.y * t };
                                        float dx = mouse2D.x - closest.x;
                                        float dy = mouse2D.y - closest.y;
                                        constexpr float kLineClickMargin = 8.0f;
                                        float hitThreshold = thickness * 0.5f + kLineClickMargin;
                                        if (dx * dx + dy * dy <= hitThreshold * hitThreshold) {
                                            nearest2DObj = obj;
                                            break;
                                        }
                                    }
                                }
                                if (auto* tr = go->GetComponent<TextRenderer2DComponent>()) {
                                    Vector3 pos = go->GetPosition();
                                    Vector3 scl = go->GetScale();
                                    float w = tr->GetWidth() * scl.x;
                                    float h = tr->GetHeight() * scl.y;
                                    if (mouse2D.x >= pos.x && mouse2D.x <= pos.x + w &&
                                        mouse2D.y >= pos.y && mouse2D.y <= pos.y + h) {
                                        nearest2DObj = obj;
                                        break;
                                    }
                                }
                            } else if (auto* sprite = dynamic_cast<SpriteObject*>(obj)) {
                                Vector3 pos = sprite->GetPosition();
                                Vector3 scl = sprite->GetScale();
                                float w = sprite->GetWidth() * scl.x;
                                float h = sprite->GetHeight() * scl.y;
                                if (mouse2D.x >= pos.x && mouse2D.x <= pos.x + w &&
                                    mouse2D.y >= pos.y && mouse2D.y <= pos.y + h) {
                                    nearest2DObj = obj;
                                    break;
                                }
                            } else if (auto* tri = dynamic_cast<Triangle2DObject*>(obj)) {
                                Vector3 pos = tri->GetPosition();
                                Vector3 scl = tri->GetScale();
                                float w = tri->GetWidth() * scl.x;
                                float h = tri->GetHeight() * scl.y;
                                if (mouse2D.x >= pos.x && mouse2D.x <= pos.x + w &&
                                    mouse2D.y >= pos.y && mouse2D.y <= pos.y + h) {
                                    nearest2DObj = obj;
                                    break;
                                }
                            } else if (auto* circle = dynamic_cast<Circle2DObject*>(obj)) {
                                Vector3 pos = circle->GetPosition();
                                Vector3 scl = circle->GetScale();
                                float r = circle->GetRadius() * scl.x;
                                float cx = pos.x + r;
                                float cy = pos.y + r;
                                float dx = mouse2D.x - cx;
                                float dy = mouse2D.y - cy;
                                if (dx * dx + dy * dy <= r * r) {
                                    nearest2DObj = obj;
                                    break;
                                }
                            } else if (auto* ring = dynamic_cast<Ring2DObject*>(obj)) {
                                Vector3 pos = ring->GetPosition();
                                Vector3 scl = ring->GetScale();
                                float outerR = ring->GetOuterRadius() * scl.x;
                                float cx = pos.x + outerR;
                                float cy = pos.y + outerR;
                                float dx = mouse2D.x - cx;
                                float dy = mouse2D.y - cy;
                                if (dx * dx + dy * dy <= outerR * outerR) {
                                    nearest2DObj = obj;
                                    break;
                                }
                            } else if (auto* line = dynamic_cast<Line2DObject*>(obj)) {
                                Vector2 start = line->GetStart();
                                Vector2 end = line->GetEnd();
                                float thickness = line->GetThickness();
                                Vector2 ab = { end.x - start.x, end.y - start.y };
                                Vector2 ap = { mouse2D.x - start.x, mouse2D.y - start.y };
                                float abLenSq = ab.x * ab.x + ab.y * ab.y;
                                float t = (abLenSq > 0.0001f) ? (ap.x * ab.x + ap.y * ab.y) / abLenSq : 0.0f;
                                t = (std::max)(0.0f, (std::min)(1.0f, t));
                                Vector2 closest = { start.x + ab.x * t, start.y + ab.y * t };
                                float dx = mouse2D.x - closest.x;
                                float dy = mouse2D.y - closest.y;
                                constexpr float kLineClickMargin = 8.0f;
                                float hitThreshold = thickness * 0.5f + kLineClickMargin;
                                if (dx * dx + dy * dy <= hitThreshold * hitThreshold) {
                                    nearest2DObj = obj;
                                    break;
                                }
                            }
                        }

                        IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();

                        if (nearest2DObj != nullptr) {
                            // 2Dオブジェクトがクリックされた場合
                            if (selected != nearest2DObj) {
                                SceneHierarchy::GetInstance()->SetSelected(nearest2DObj);
                            }
                        } else {
                            // ----------------------------------------------------
                            // 2. 2Dにヒットしない場合のみ 3Dレイキャスト判定を実行
                            // ----------------------------------------------------
                            Vector3 rayOrigin, rayDir;
                            camera->CreateRay(relativeMousePos, width, height, rayOrigin, rayDir);

                            IGameObject* nearestObj = nullptr;
                            float minDistance = FLT_MAX;

                            for (auto* obj : objects) {
                                LineObject* lineObj = dynamic_cast<LineObject*>(obj);
                                Object3D* target3D = dynamic_cast<Object3D*>(obj);
                                BaseCamera* camObj = dynamic_cast<BaseCamera*>(obj);
                                DirectionalLightObject* lightObj = dynamic_cast<DirectionalLightObject*>(obj);

                                if (lineObj) {
                                    // Lineの場合は始点と終点のそれぞれで距離判定を行う
                                    Vector3 points[2] = { lineObj->GetStartPoint(), lineObj->GetEndPoint() };
                                    constexpr float lineRadius = 0.5f;

                                    for (const auto& pt : points) {
                                        Vector3 v = Math::Subtract(pt, rayOrigin);
                                        float tProj = Math::Dot(v, rayDir);
                                        if (tProj >= 0.0f) {
                                            Vector3 projPt = { rayOrigin.x + rayDir.x * tProj, rayOrigin.y + rayDir.y * tProj, rayOrigin.z + rayDir.z * tProj };
                                            float d = Math::Length(Math::Subtract(projPt, pt));
                                            if (d <= lineRadius) {
                                                if (d < minDistance) {
                                                    minDistance = d;
                                                    nearestObj = obj;
                                                }
                                            }
                                        }
                                    }
                                } else if (camObj || lightObj) {
                                    if (camObj && camObj == camera) continue;

                                    Vector3 pos = camObj ? camObj->GetPosition() : lightObj->GetPosition();
                                    constexpr float radius = 0.5f;

                                    Vector3 v = Math::Subtract(pos, rayOrigin);
                                    float tProj = Math::Dot(v, rayDir);
                                    if (tProj >= 0.0f) {
                                        Vector3 projPt = { rayOrigin.x + rayDir.x * tProj, rayOrigin.y + rayDir.y * tProj, rayOrigin.z + rayDir.z * tProj };
                                        float d = Math::Length(Math::Subtract(projPt, pos));
                                        if (d <= radius) {
                                            if (d < minDistance) {
                                                minDistance = d;
                                                nearestObj = obj;
                                            }
                                        }
                                    }
                                } else if (target3D) {
                                    Vector3 pos = target3D->GetPosition();
                                    Vector3 scale = target3D->GetScale();
                                    
                                    constexpr float kMinHitRadius = 0.3f;
                                    float radius = (scale.x + scale.y + scale.z) / 3.0f;
                                    if (radius < kMinHitRadius) radius = kMinHitRadius;

                                    Vector3 v = Math::Subtract(pos, rayOrigin);
                                    float tProj = Math::Dot(v, rayDir);
                                    if (tProj >= 0.0f) {
                                        Vector3 projPt = { rayOrigin.x + rayDir.x * tProj, rayOrigin.y + rayDir.y * tProj, rayOrigin.z + rayDir.z * tProj };
                                        float d = Math::Length(Math::Subtract(projPt, pos));
                                        if (d <= radius) {
                                            if (d < minDistance) {
                                                minDistance = d;
                                                nearestObj = obj;
                                            }
                                        }
                                    }
                                } else if (auto* targetGo = dynamic_cast<GameObject*>(obj)) {
                                    Vector3 pos = targetGo->GetPosition();
                                    Vector3 scale = targetGo->GetScale();

                                    constexpr float kMinHitRadius = 0.4f;
                                    float radius = (scale.x + scale.y + scale.z) / 3.0f;
                                    if (radius < kMinHitRadius) radius = kMinHitRadius;

                                    Vector3 v = Math::Subtract(pos, rayOrigin);
                                    float tProj = Math::Dot(v, rayDir);
                                    if (tProj >= 0.0f) {
                                        Vector3 projPt = { rayOrigin.x + rayDir.x * tProj, rayOrigin.y + rayDir.y * tProj, rayOrigin.z + rayDir.z * tProj };
                                        float d = Math::Length(Math::Subtract(projPt, pos));
                                        if (d <= radius) {
                                            if (d < minDistance) {
                                                minDistance = d;
                                                nearestObj = obj;
                                            }
                                        }
                                    }
                                }
                            }

                            if (nearestObj == nullptr) {
                                // 背景の空スペースをクリックした場合は選択解除
                                SceneHierarchy::GetInstance()->SetSelected(nullptr);
                            } else {
                                if (selected != nearestObj) {
                                    SceneHierarchy::GetInstance()->SetSelected(nearestObj);
                                }
                            }
                        }
                    }
                }
            }

            // ImGuizmo の描画・操作処理
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            if (isGizmoEffective && selected && camera) {
                ImGuizmo::GetStyle().HatchedAxisLineThickness = kDisabledHatchedAxisThickness;
                Line2DObject* targetLine2D = dynamic_cast<Line2DObject*>(selected);
                SpriteObject* targetSprite = dynamic_cast<SpriteObject*>(selected);
                Triangle2DObject* targetTri2D = dynamic_cast<Triangle2DObject*>(selected);
                Circle2DObject* targetCircle2D = dynamic_cast<Circle2DObject*>(selected);
                Ring2DObject* targetRing2D = dynamic_cast<Ring2DObject*>(selected);

                LineObject* targetLine = dynamic_cast<LineObject*>(selected);
                Object3D* target3D = dynamic_cast<Object3D*>(selected);
                BaseCamera* targetCam = dynamic_cast<BaseCamera*>(selected);
                DirectionalLightObject* targetLight = dynamic_cast<DirectionalLightObject*>(selected);
                GameObject* targetGo = dynamic_cast<GameObject*>(selected);
                const bool isTargetGo2D = targetGo && targetGo->Is2D();

                // Manipulate 実行前の初期状態を退避
                Transform initialTr{};
                bool hasInitialTr = false;
                Vector2 initialLineStart{};
                Vector2 initialLineEnd{};
                if (targetLine2D) {
                    initialLineStart = targetLine2D->GetStart();
                    initialLineEnd = targetLine2D->GetEnd();
                } else {
                    hasInitialTr = GetObjectTransform(selected, initialTr);
                }

                bool isUsingGizmoThisFrame = false;

                if (targetLine2D) {
                    // 2Dラインギズモ（正射影）
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(true);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix2D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix2D();

                    // 1. 始点ギズモ
                    ImGuizmo::PushID(0);
                    Vector2 start2D = targetLine2D->GetStart();
                    Vector3 startPos3D = { start2D.x, start2D.y, 0.0f };
                    const Vector3 defaultLineScale = { 1.0f, 1.0f, 1.0f };
                    const Vector3 defaultLineRotate = { 0.0f, 0.0f, 0.0f };
                    Matrix4x4 worldMatStart = Math::MakeAffineMatrix(defaultLineScale, defaultLineRotate, startPos3D);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMatStart.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rot[3], scl[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMatStart.m[0][0], translation, rot, scl);
                        targetLine2D->SetPoints({ translation[0], translation[1] }, targetLine2D->GetEnd());
                    }
                    ImGuizmo::PopID();

                    // 2. 終点ギズモ
                    ImGuizmo::PushID(1);
                    Vector2 end2D = targetLine2D->GetEnd();
                    Vector3 endPos3D = { end2D.x, end2D.y, 0.0f };
                    Matrix4x4 worldMatEnd = Math::MakeAffineMatrix(defaultLineScale, defaultLineRotate, endPos3D);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMatEnd.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rot[3], scl[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMatEnd.m[0][0], translation, rot, scl);
                        targetLine2D->SetPoints(targetLine2D->GetStart(), { translation[0], translation[1] });
                    }
                    ImGuizmo::PopID();

                } else if (targetSprite || targetTri2D || targetCircle2D || targetRing2D || isTargetGo2D) {
                    // 2Dオブジェクトギズモ（正射影）
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(true);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix2D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix2D();

                    Vector3 scale = targetSprite ? targetSprite->GetScale() :
                                    targetTri2D ? targetTri2D->GetScale() :
                                    targetCircle2D ? targetCircle2D->GetScale() :
                                    targetRing2D ? targetRing2D->GetScale() : targetGo->GetScale();
                    Vector3 rotate = targetSprite ? targetSprite->GetRotate() :
                                     targetTri2D ? targetTri2D->GetRotate() :
                                     targetCircle2D ? targetCircle2D->GetRotate() :
                                     targetRing2D ? targetRing2D->GetRotate() : targetGo->GetRotate();
                    Vector3 position = targetSprite ? targetSprite->GetPosition() :
                                       targetTri2D ? targetTri2D->GetPosition() :
                                       targetCircle2D ? targetCircle2D->GetPosition() :
                                       targetRing2D ? targetRing2D->GetPosition() : targetGo->GetPosition();

                    Matrix4x4 worldMat = Math::MakeAffineMatrix(scale, rotate, position);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMat.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotationComponents, scaleComponents);

                        Vector3 newPos = { translation[0], translation[1], 0.0f };
                        constexpr float kDegToRad = 3.1415926535f / 180.0f;
                        Vector3 newRot = { 0.0f, 0.0f, rotationComponents[2] * kDegToRad };
                        Vector3 newScale = { scaleComponents[0], scaleComponents[1], 1.0f };

                        if (targetSprite) {
                            targetSprite->SetPosition(newPos);
                            targetSprite->SetRotate(newRot);
                            targetSprite->SetScale(newScale);
                        } else if (targetTri2D) {
                            targetTri2D->SetPosition(newPos);
                            targetTri2D->SetRotate(newRot);
                            targetTri2D->SetScale(newScale);
                        } else if (targetCircle2D) {
                            targetCircle2D->SetPosition(newPos);
                            targetCircle2D->SetRotate(newRot);
                            targetCircle2D->SetScale(newScale);
                        } else if (targetRing2D) {
                            targetRing2D->SetPosition(newPos);
                            targetRing2D->SetRotate(newRot);
                            targetRing2D->SetScale(newScale);
                        } else if (targetGo) {
                            targetGo->SetPosition(newPos);
                            targetGo->SetRotate(newRot);
                            targetGo->SetScale(newScale);
                            targetGo->UpdateMatrix();
                        }
                    }

                } else if (targetLine) {
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    // カメラの行列取得
                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix3D();

                    // 1. 始点 (Start Point) ギズモ
                    ImGuizmo::PushID(0);
                    Vector3 startPos = targetLine->GetStartPoint();
                    Matrix4x4 worldMatStart = Math::MakeAffineMatrix({1,1,1}, {0,0,0}, startPos);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMatStart.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMatStart.m[0][0], translation, rotationComponents, scaleComponents);
                        targetLine->SetStartPoint({ translation[0], translation[1], translation[2] });
                    }
                    ImGuizmo::PopID();

                    // 2. 終点 (End Point) ギズモ
                    ImGuizmo::PushID(1);
                    Vector3 endPos = targetLine->GetEndPoint();
                    Matrix4x4 worldMatEnd = Math::MakeAffineMatrix({1,1,1}, {0,0,0}, endPos);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMatEnd.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMatEnd.m[0][0], translation, rotationComponents, scaleComponents);
                        targetLine->SetEndPoint({ translation[0], translation[1], translation[2] });
                    }
                    ImGuizmo::PopID();

                } else if (targetCam || targetLight) {
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix3D();

                    Vector3 position = targetCam ? targetCam->GetPosition() : targetLight->GetPosition();
                    Vector3 rotate = targetCam ? targetCam->GetRotation() : targetLight->GetRotate();
                    Vector3 scale = { 1.0f, 1.0f, 1.0f };

                    Matrix4x4 worldMat = Math::MakeAffineMatrix(scale, rotate, position);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMat.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotationComponents, scaleComponents);

                        Vector3 newPos = { translation[0], translation[1], translation[2] };
                        constexpr float kDegToRad = 3.1415926535f / 180.0f;
                        Vector3 newRot = {
                            rotationComponents[0] * kDegToRad,
                            rotationComponents[1] * kDegToRad,
                            rotationComponents[2] * kDegToRad
                        };

                        if (targetCam) {
                            targetCam->SetPosition(newPos);
                            targetCam->SetRotation(newRot);
                        } else if (targetLight) {
                            targetLight->SetPosition(newPos);
                            targetLight->SetRotate(newRot);
                        }
                    }

                } else if (target3D) {
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    // カメラの行列取得
                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix3D();

                    // オブジェクトのパラメータを取得
                    Vector3 scale = target3D->GetScale();
                    Vector3 rotate = target3D->GetRotate();
                    Vector3 position = target3D->GetPosition();

                    // ワールド行列を算出
                    Matrix4x4 worldMat = Math::MakeAffineMatrix(scale, rotate, position);

                    // ギズモ操作 (現在の操作モードを適用)
                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMat.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotationComponents, scaleComponents);

                        // 更新されたトランスフォームをオブジェクトへ再適用 (度数法からラジアン変換)
                        Vector3 newPos = { translation[0], translation[1], translation[2] };
                        constexpr float kDegToRad = 3.1415926535f / 180.0f;
                        Vector3 newRot = {
                            rotationComponents[0] * kDegToRad,
                            rotationComponents[1] * kDegToRad,
                            rotationComponents[2] * kDegToRad
                        };
                        Vector3 newScale = { scaleComponents[0], scaleComponents[1], scaleComponents[2] };

                        target3D->SetPosition(newPos);
                        target3D->SetRotate(newRot);
                        target3D->SetScale(newScale);
                    }
                } else if (targetGo && !isTargetGo2D) {
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix3D();

                    Vector3 scale = targetGo->GetScale();
                    Vector3 rotate = targetGo->GetRotate();
                    Vector3 position = targetGo->GetPosition();

                    Matrix4x4 worldMat = Math::MakeAffineMatrix(scale, rotate, position);

                    ImGuizmo::Manipulate(
                        &viewMat.m[0][0],
                        &projMat.m[0][0],
                        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
                        ImGuizmo::LOCAL,
                        &worldMat.m[0][0]
                    );

                    if (ImGuizmo::IsUsing()) {
                        isUsingGizmoThisFrame = true;
                        float translation[3], rotationComponents[3], scaleComponents[3];
                        ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], translation, rotationComponents, scaleComponents);

                        Vector3 newPos = { translation[0], translation[1], translation[2] };
                        constexpr float kDegToRad = 3.1415926535f / 180.0f;
                        Vector3 newRot = {
                            rotationComponents[0] * kDegToRad,
                            rotationComponents[1] * kDegToRad,
                            rotationComponents[2] * kDegToRad
                        };
                        Vector3 newScale = { scaleComponents[0], scaleComponents[1], scaleComponents[2] };

                        targetGo->SetPosition(newPos);
                        targetGo->SetRotate(newRot);
                        targetGo->SetScale(newScale);
                    }
                }

                // ギズモ操作開始の検知（ドラッグ開始フレーム）
                if (isUsingGizmoThisFrame && !sWasUsingGizmo) {
                    sBeforeTargetName = selected->GetName();
                    if (targetLine2D) {
                        sBeforeLineStart = initialLineStart;
                        sBeforeLineEnd = initialLineEnd;
                        sIsTrackingLine2D = true;
                        sIsTrackingTransform = false;
                    } else if (hasInitialTr) {
                        sBeforeTransform = initialTr;
                        sIsTrackingTransform = true;
                        sIsTrackingLine2D = false;
                    }
                }

                // ギズモ操作終了の検知（マウスを離した瞬間）
                if (!isUsingGizmoThisFrame && sWasUsingGizmo) {
                    if (sIsTrackingTransform && selected->GetName() == sBeforeTargetName) {
                        Transform currentTr{};
                        if (GetObjectTransform(selected, currentTr)) {
                            bool changed = (std::abs(currentTr.translate.x - sBeforeTransform.translate.x) > kTransformChangeThreshold ||
                                            std::abs(currentTr.translate.y - sBeforeTransform.translate.y) > kTransformChangeThreshold ||
                                            std::abs(currentTr.translate.z - sBeforeTransform.translate.z) > kTransformChangeThreshold ||
                                            std::abs(currentTr.rotate.x - sBeforeTransform.rotate.x) > kTransformChangeThreshold ||
                                            std::abs(currentTr.rotate.y - sBeforeTransform.rotate.y) > kTransformChangeThreshold ||
                                            std::abs(currentTr.rotate.z - sBeforeTransform.rotate.z) > kTransformChangeThreshold ||
                                            std::abs(currentTr.scale.x - sBeforeTransform.scale.x) > kTransformChangeThreshold ||
                                            std::abs(currentTr.scale.y - sBeforeTransform.scale.y) > kTransformChangeThreshold ||
                                            std::abs(currentTr.scale.z - sBeforeTransform.scale.z) > kTransformChangeThreshold);
                            if (changed) {
                                CommandHistory::GetInstance()->PushCommand(
                                    std::make_unique<TransformCommand>(sBeforeTargetName, sBeforeTransform, currentTr)
                                );
                            }
                        }
                        sIsTrackingTransform = false;
                    } else if (sIsTrackingLine2D && targetLine2D && selected->GetName() == sBeforeTargetName) {
                        Vector2 currentStart = targetLine2D->GetStart();
                        Vector2 currentEnd = targetLine2D->GetEnd();
                        bool changed = (std::abs(currentStart.x - sBeforeLineStart.x) > kTransformChangeThreshold ||
                                        std::abs(currentStart.y - sBeforeLineStart.y) > kTransformChangeThreshold ||
                                        std::abs(currentEnd.x - sBeforeLineEnd.x) > kTransformChangeThreshold ||
                                        std::abs(currentEnd.y - sBeforeLineEnd.y) > kTransformChangeThreshold);
                        if (changed) {
                            CommandHistory::GetInstance()->PushCommand(
                                std::make_unique<LinePointsCommand>(sBeforeTargetName, sBeforeLineStart, sBeforeLineEnd, currentStart, currentEnd)
                            );
                        }
                        sIsTrackingLine2D = false;
                    }
                    sBeforeTargetName.clear();
                }

                sWasUsingGizmo = isUsingGizmoThisFrame;
            } else {
                // ギズモ操作対象がないか無効な状態になった場合は状態をリセット
                sWasUsingGizmo = false;
                sIsTrackingTransform = false;
                sIsTrackingLine2D = false;
                sBeforeTargetName.clear();
            }

        } else {
            ImGui::Text("No Active PostProcess");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    } else {
        sIsMouseOnSceneView_ = false;
    }
    ImGui::End();
}
#endif

// -------------------------------------------------------------
// 静的関数の実装（_USEIMGUIの定義に関わらず利用可能）
// -------------------------------------------------------------

bool SceneViewWindow::IsMouseOnSceneView() {
#ifdef _USEIMGUI
    return sIsMouseOnSceneView_;
#else
    return true; // ImGuiが無ければ画面全体がゲーム画面なので常にtrue
#endif
}

Vector2 SceneViewWindow::GetSceneViewSize() {
#ifdef _USEIMGUI
    return sSceneViewSize_;
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

Vector2 SceneViewWindow::GetMousePosition() {
#ifdef _USEIMGUI
    ImVec2 mousePos = ImGui::GetMousePos();
    Vector2 localPos = Vector2{ mousePos.x - sSceneViewPosMin_.x, mousePos.y - sSceneViewPosMin_.y };
    if (sSceneViewSize_.x > 0.0f && sSceneViewSize_.y > 0.0f) {
        localPos.x = std::clamp(localPos.x, 0.0f, sSceneViewSize_.x);
        localPos.y = std::clamp(localPos.y, 0.0f, sSceneViewSize_.y);
    }
    return localPos;
#else
    // ウィンドウのクライアント領域上のマウス座標を取得
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();
    POINT point;
    if (GetCursorPos(&point) && ScreenToClient(hwnd, &point)) {
        Vector2 localPos = Vector2{ static_cast<float>(point.x), static_cast<float>(point.y) };
        Vector2 viewSize = GetSceneViewSize();
        if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
            localPos.x = std::clamp(localPos.x, 0.0f, viewSize.x);
            localPos.y = std::clamp(localPos.y, 0.0f, viewSize.y);
        }
        return localPos;
    }
    return Vector2{ 0.0f, 0.0f };
#endif
}

Vector2 SceneViewWindow::GetSceneViewPosMin() {
#ifdef _USEIMGUI
    return sSceneViewPosMin_;
#else
    return Vector2{ 0.0f, 0.0f };
#endif
}

bool SceneViewWindow::WorldToScreen(const Vector3& worldPos, const BaseCamera* camera, Vector2& outScreenPos, float vpOffsetXRatio, float vpWidthRatio) {
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
    Vector2 viewPos = GetSceneViewPosMin();
    Vector2 viewSize = GetSceneViewSize();

    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        return false;
    }

    // NDC [-1, 1] をスクリーンピクセル座標へ変換 (DirectX系のY軸反転を考慮、ビューポートオフセット・幅比率を適用)
    float localScreenX = (vpOffsetXRatio + (ndcX * kHalfCoord + kHalfCoord) * vpWidthRatio) * viewSize.x;
    outScreenPos.x = viewPos.x + localScreenX;
    outScreenPos.y = viewPos.y + (-ndcY * kHalfCoord + kHalfCoord) * viewSize.y;
    return true;
}

