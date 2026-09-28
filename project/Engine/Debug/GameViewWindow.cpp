#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/WindowApp/WindowApp.h"
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
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Input/Input.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <algorithm>

namespace {
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

GameViewWindow::GameViewWindow()
    : wasPaused_(false), showGizmo_(true) {
}

void GameViewWindow::Draw(bool* show, bool* isVisible) {
    *isVisible = false;
    
    if (ImGui::Begin("ゲーム画面###Game View", show)) {
        *isVisible = true;

        // 1. ギズモ操作モード切り替えUI（ImGuizmo OPERATION 定数）
        constexpr int kGizmoOpTranslate = 7;
        constexpr int kGizmoOpRotate = 120;
        constexpr int kGizmoOpScale = 896;

        int currentOp = gizmoOperation_;
        bool opChanged = false;

        ImGui::Text("ギズモ操作:");
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

        ImGui::Separator();

        // 2. ゲーム画面描画エリア（内部のみWindowPaddingを0にする）
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

            // 3Dレイキャストによるオブジェクト直接選択処理
            BaseCamera* camera = CameraResource::GetCameraManager()->GetActiveCamera();
            if (camera && sIsMouseOnGameView_) {
                // ギズモの操作子をホバー中・操作中、あるいはCtrlキー押下中の場合はレイキャストを一切行わない（誤クリック防止）
                auto input = InputResource::GetInput();
                bool isCtrlPressed = input && (input->Press(DIK_LCONTROL) || input->Press(DIK_RCONTROL));
                bool isGizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing() || isCtrlPressed;

                if (!isGizmoActive) {
                    bool isLeftClicked = ImGui::IsMouseClicked(0);
                    bool isRightClicked = ImGui::IsMouseClicked(1);
                    bool isMiddleClicked = ImGui::IsMouseClicked(2);

                    if (isLeftClicked || isRightClicked || isMiddleClicked) {
                        Vector2 relativeMousePos = GetMousePosition();
                        Vector3 rayOrigin, rayDir;
                        camera->CreateRay(relativeMousePos, width, height, rayOrigin, rayDir);

                        const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
                        IGameObject* nearestObj = nullptr;
                        float minDistance = FLT_MAX;

                        for (auto* obj : objects) {
                            LineObject* lineObj = dynamic_cast<LineObject*>(obj);
                            Object3D* target3D = dynamic_cast<Object3D*>(obj);
                            SpriteObject* targetSprite = dynamic_cast<SpriteObject*>(obj);
                            BaseCamera* camObj = dynamic_cast<BaseCamera*>(obj);
                            DirectionalLightObject* lightObj = dynamic_cast<DirectionalLightObject*>(obj);

                            if (lineObj) {
                                // Lineの場合は始点と終点のそれぞれで距離判定を行う
                                Vector3 points[2] = { lineObj->GetStartPoint(), lineObj->GetEndPoint() };
                                float lineRadius = 0.5f;

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
                                // 自分自身（現在のアクティブカメラ、通常Editorカメラ）は選択対象外とする
                                if (camObj && camObj == camera) continue;

                                Vector3 pos = camObj ? camObj->GetPosition() : lightObj->GetPosition();
                                float radius = 0.5f; // 簡易的な当たり判定サイズ

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
                            } else if (target3D || targetSprite) {
                                Vector3 pos = target3D ? target3D->GetPosition() : targetSprite->GetPosition();
                                Vector3 scale = target3D ? target3D->GetScale() : targetSprite->GetScale();
                                
                                // 半径はスケールの平均値とする（最小でも0.3f）
                                float radius = (scale.x + scale.y + scale.z) / 3.0f;
                                if (radius < 0.3f) radius = 0.3f;

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

                        IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();

                        if (nearestObj == nullptr) {
                            // 背景の空スペースをクリックした場合は選択解除
                            SceneHierarchy::GetInstance()->SetSelected(nullptr);
                        } else {
                            // 何かがヒットした場合
                            if (selected == nearestObj) {
                                // 同一オブジェクトが再度クリックされた場合は、誤操作防止のため何も変更しない
                            } else {
                                // 異なるオブジェクトがクリックされた（または何も選択されていなかった）場合は、そのターゲットを選択してモードを適用
                                SceneHierarchy::GetInstance()->SetSelected(nearestObj);
                                if (isLeftClicked) {
                                    gizmoOperation_ = 7; // TRANSLATE
                                } else if (isMiddleClicked) {
                                    gizmoOperation_ = 120; // ROTATE
                                } else if (isRightClicked) {
                                    gizmoOperation_ = 896; // SCALE
                                }
                            }
                        }
                    }
                }
            }

            // ImGuizmo の描画・操作処理
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            if (showGizmo_ && selected && camera) {
                LineObject* targetLine = dynamic_cast<LineObject*>(selected);
                Object3D* target3D = dynamic_cast<Object3D*>(selected);
                SpriteObject* targetSprite = dynamic_cast<SpriteObject*>(selected);
                BaseCamera* targetCam = dynamic_cast<BaseCamera*>(selected);
                DirectionalLightObject* targetLight = dynamic_cast<DirectionalLightObject*>(selected);

                if (targetLine) {
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

                } else if (target3D || targetSprite) {
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imgPosMin.x, imgPosMin.y, width, height);

                    // カメラの行列取得
                    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
                    Matrix4x4 projMat = CameraResource::GetCameraManager()->GetProjectionMatrix3D();

                    // オブジェクトのパラメータを取得
                    Vector3 scale = target3D ? target3D->GetScale() : targetSprite->GetScale();
                    Vector3 rotate = target3D ? target3D->GetRotate() : targetSprite->GetRotate();
                    Vector3 position = target3D ? target3D->GetPosition() : targetSprite->GetPosition();

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

                        if (target3D) {
                            target3D->SetPosition(newPos);
                            target3D->SetRotate(newRot);
                            target3D->SetScale(newScale);
                        } else if (targetSprite) {
                            targetSprite->SetPosition(newPos);
                            targetSprite->SetRotate(newRot);
                            targetSprite->SetScale(newScale);
                        }
                    }
                }
            }

            // 中央座標の計算
            ImVec2 center = ImVec2(imgPosMin.x + width * 0.5f, imgPosMin.y + height * 0.5f);

            // アニメーションの更新と描画
            popAnim_.Update(ImGui::GetIO().DeltaTime);
            popAnim_.Draw(ImGui::GetWindowDrawList(), center);

            // リプレイシステム削除に伴い、クリックポーズおよびポーズ中オーバーレイ表示は無効化されました

        } else {
            ImGui::Text("No Active PostProcess");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
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
