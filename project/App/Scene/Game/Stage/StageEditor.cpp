#include "App/Scene/Game/Stage/StageEditor.h"
#include "App/Scene/Game/Camera/DrawRouteCamera.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Debug/DebugEditor.h"
#include "Engine/Input/Input.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <direct.h> // CreateDirectoryA用

namespace {
    // 3Dラベル・バッジ描画用の色定数 (マジックナンバー排除)
    static constexpr ImU32 kBadgeBgColor = IM_COL32(20, 20, 25, 215);             // バッジ背景色（半透明ダーク）
    static constexpr ImU32 kBadgeBorderColor = IM_COL32(140, 140, 160, 200);     // バッジ枠線色
    static constexpr ImU32 kBadgeSelectedBorderColor = IM_COL32(255, 230, 40, 255); // 選択中バッジ枠線色（発光黄）
    static constexpr ImU32 kTextColor = IM_COL32(255, 255, 255, 255);            // 通常テキスト色
    static constexpr ImU32 kSelectedTextColor = IM_COL32(255, 240, 100, 255);    // 選択中テキスト色
    static constexpr ImU32 kLineColor = IM_COL32(200, 200, 220, 160);            // 地面への引き出し線色

    // 湧き範囲サークル描画用の色定数 (マジックナンバー排除)
    static constexpr ImU32 kNormalCircleColor = IM_COL32(255, 70, 70, 220);        // 通常突進（赤）
    static constexpr ImU32 kNormalCircleFillColor = IM_COL32(255, 70, 70, 35);
    static constexpr ImU32 kStationaryCircleColor = IM_COL32(255, 170, 40, 220);    // 固定砲台（橙）
    static constexpr ImU32 kStationaryCircleFillColor = IM_COL32(255, 170, 40, 35);
    static constexpr ImU32 kSwarmCircleColor = IM_COL32(60, 220, 255, 220);        // 追従群生（シアン）
    static constexpr ImU32 kSwarmCircleFillColor = IM_COL32(60, 220, 255, 35);
    static constexpr ImU32 kBossCircleColor = IM_COL32(230, 60, 240, 240);          // BOSS（紫）
    static constexpr ImU32 kBossCircleFillColor = IM_COL32(230, 60, 240, 45);
    static constexpr ImU32 kSelectedCircleColor = IM_COL32(255, 255, 50, 255);      // 選択中（発光黄）
    static constexpr ImU32 kSelectedCircleFillColor = IM_COL32(255, 255, 50, 50);

    // 3D配置プレビュー用の色定数 (マジックナンバー排除)
    static constexpr ImU32 kPreviewCircleColor = IM_COL32(50, 255, 120, 240);       // 配置プレビュー枠線（発光ライムグリーン）
    static constexpr ImU32 kPreviewCircleFillColor = IM_COL32(50, 255, 120, 45);     // 配置プレビュー塗りつぶし
    static constexpr ImU32 kPreviewBadgeBgColor = IM_COL32(20, 40, 25, 230);        // 配置プレビューバッジ背景
    static constexpr ImU32 kPreviewBadgeBorderColor = IM_COL32(60, 255, 130, 255);   // 配置プレビューバッジ枠線
    static constexpr ImU32 kPreviewTextColor = IM_COL32(160, 255, 180, 255);        // 配置プレビューテキスト色
}

void StageEditor::Initialize() {
    editorEnemies_.clear();
    selectedEnemyIndex_ = -1;
    showStageEditor_ = true;
    showSpawnCircles_ = true;
    show3DLabels_ = true;
    showDuringPlay_ = true;
}

void StageEditor::SetSelectedIndex(int index) {
    selectedEnemyIndex_ = index;
    for (int i = 0; i < static_cast<int>(editorEnemies_.size()); ++i) {
        if (editorEnemies_[i]) {
            editorEnemies_[i]->SetSelectedInEditor(i == selectedEnemyIndex_);
        }
    }

    // 選択された敵が存在する場合、カメラをその位置へ自動フォーカス移動
    if (selectedEnemyIndex_ >= 0 && selectedEnemyIndex_ < static_cast<int>(editorEnemies_.size())) {
        if (const auto& targetEnemy = editorEnemies_[selectedEnemyIndex_]) {
            Vector3 enemyPos = targetEnemy->GetPosition();
            if (drawRouteCamera_) {
                drawRouteCamera_->SetDestinationZoom(enemyPos);
            }
            if (auto cameraMgr = CameraResource::GetCameraManager()) {
                if (auto activeCam = cameraMgr->GetActiveCamera()) {
                    if (activeCam->IsUseTarget()) {
                        activeCam->SetTarget(enemyPos);
                    }
                }
            }
        }
    }
}

void StageEditor::Update() {
#ifdef _USEIMGUI
    sIsPlacingNow_ = (editMode_ == EditMode::Placing);

    if (selectedEnemyIndex_ < 0 || selectedEnemyIndex_ >= static_cast<int>(editorEnemies_.size())) {
        selectedEnemyIndex_ = -1;
    }

    // ギズモでオブジェクトが選択された場合の双方向同期
    auto selectedObj = SceneHierarchy::GetInstance()->GetSelected();
    if (selectedObj) {
        bool found = false;
        for (int i = 0; i < static_cast<int>(editorEnemies_.size()); ++i) {
            if (editorEnemies_[i] && editorEnemies_[i]->GetCube() == selectedObj) {
                selectedEnemyIndex_ = i;
                found = true;
                break;
            }
        }
        if (!found) {
            // 敵以外のオブジェクトが選択された場合はリストの選択を外す
            selectedEnemyIndex_ = -1;
        }
    } else {
        selectedEnemyIndex_ = -1;
    }

    // 各敵オブジェクトの更新および選択状態反映
    for (int i = 0; i < static_cast<int>(editorEnemies_.size()); ++i) {
        if (editorEnemies_[i]) {
            editorEnemies_[i]->SetSelectedInEditor(i == selectedEnemyIndex_);
            editorEnemies_[i]->Update();
        }
    }
#endif
}

#ifdef _USEIMGUI
void StageEditor::Update3DPlacement(BaseCamera* camera) {
    isCursorOnGround_ = false;

    if (editMode_ != EditMode::Placing || !camera) {
        return;
    }

    if (!GameViewWindow::IsMouseOnGameView()) {
        return;
    }

    Vector2 mousePos = GameViewWindow::GetMousePosition();
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
        return;
    }

    float viewLeft = viewSize.x * vpOffsetXRatio_;
    float viewW = viewSize.x * vpWidthRatio_;
    if (viewW <= 0.0f) return;

    // マウスが 3D ビューポート領域内にあるか判定
    if (mousePos.x < viewLeft || mousePos.x > viewLeft + viewW) {
        return;
    }

    // 3D ビューポート内での相対マウス座標
    Vector2 mouseIn3D = { mousePos.x - viewLeft, mousePos.y };

    Vector3 rayOrigin, rayDir;
    camera->CreateRay(mouseIn3D, viewW, viewSize.y, rayOrigin, rayDir);

    // 地面（Y = kGroundLevelY）との交差判定
    if (std::abs(rayDir.y) > kRayDirThreshold) {
        float t = (kGroundLevelY - rayOrigin.y) / rayDir.y;
        if (t > 0.0f) {
            isCursorOnGround_ = true;
            cursorGroundPos_ = {
                rayOrigin.x + rayDir.x * t,
                kGroundLevelY,
                rayOrigin.z + rayDir.z * t
            };

            // 1. マウスホイールによるスポーン半径の調整
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                pendingRadius_ = std::clamp(
                    pendingRadius_ + (wheel > 0.0f ? kRadiusWheelStep : -kRadiusWheelStep),
                    kMinSpawnRadius,
                    kMaxSpawnRadius
                );
            }

            // 2. 左クリックによる敵の配置 (一時停止中のみ実行)
            bool isPaused = false;
            if (auto debugEditor = Zuizui::GetInstance()->GetDebugEditor()) {
                isPaused = debugEditor->IsPaused();
            }

            if (isPaused && ImGui::IsMouseClicked(0)) {
                AddEnemyFrom2D(cursorGroundPos_, pendingType_, pendingCount_, pendingRadius_, pendingHangTime_);
            }
        }
    }
}
#endif

void StageEditor::Draw3D() {
    if (!showSpawnCircles_) return;

    for (const auto& enemy : editorEnemies_) {
        if (enemy) {
            enemy->Draw();
        }
    }
}

#ifdef _USEIMGUI
void StageEditor::Draw3DLabels() {
    if (!show3DLabels_ && !showSpawnCircles_) return;

    BaseCamera* camera = CameraResource::GetCameraManager()->GetActiveCamera();
    if (!camera) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    Vector2 gameViewPos = GameViewWindow::GetGameViewPosMin();
    Vector2 gameViewSize = GameViewWindow::GetGameViewSize();
    if (gameViewSize.x <= 0.0f || gameViewSize.y <= 0.0f) return;

    float clipLeft = gameViewPos.x + gameViewSize.x * vpOffsetXRatio_;
    float clipRight = clipLeft + gameViewSize.x * vpWidthRatio_;
    ImVec2 clipMin = ImVec2(clipLeft, gameViewPos.y);
    ImVec2 clipMax = ImVec2(clipRight, gameViewPos.y + gameViewSize.y);
    drawList->PushClipRect(clipMin, clipMax, true);

    Vector3 camPos = camera->GetPosition();

    for (int i = 0; i < static_cast<int>(editorEnemies_.size()); ++i) {
        const auto& enemy = editorEnemies_[i];
        if (!enemy) continue;

        Vector3 pos = enemy->GetPosition();
        float distToCam = Math::Length(Math::Subtract(pos, camPos));
        if (distToCam > kMaxLabelVisibleDist) continue;

        bool isSelected = (i == selectedEnemyIndex_);

        // --------------------------------------------------
        // 1. 湧き範囲サークルの投影描画 (showSpawnCircles_ が有効な場合)
        // --------------------------------------------------
        if (showSpawnCircles_) {
            float spawnRadius = enemy->GetSpawnRadius();
            if (spawnRadius > 0.0f) {
                // 敵タイプに応じたサークルカラーの決定
                ImU32 circleColor = kNormalCircleColor;
                ImU32 fillColor = kNormalCircleFillColor;
                switch (enemy->GetEnemyType()) {
                case Enemy::EnemyType::Normal:
                    circleColor = kNormalCircleColor;
                    fillColor = kNormalCircleFillColor;
                    break;
                case Enemy::EnemyType::Stationary:
                    circleColor = kStationaryCircleColor;
                    fillColor = kStationaryCircleFillColor;
                    break;
                case Enemy::EnemyType::Swarm:
                    circleColor = kSwarmCircleColor;
                    fillColor = kSwarmCircleFillColor;
                    break;
                case Enemy::EnemyType::Boss:
                    circleColor = kBossCircleColor;
                    fillColor = kBossCircleFillColor;
                    break;
                }

                if (isSelected) {
                    circleColor = kSelectedCircleColor;
                    fillColor = kSelectedCircleFillColor;
                }

                // 円周上の各頂点をワールド座標からスクリーン座標へ変換
                std::vector<ImVec2> screenPoints;
                screenPoints.reserve(kCircleSegments);
                bool allPointsVisible = true;

                static constexpr float kTwoPi = 6.28318530718f;
                float angleStep = kTwoPi / static_cast<float>(kCircleSegments);

                for (int s = 0; s < kCircleSegments; ++s) {
                    float theta = static_cast<float>(s) * angleStep;
                    Vector3 circleWorldPos = {
                        pos.x + spawnRadius * std::cos(theta),
                        pos.y + kCircleElevation,
                        pos.z + spawnRadius * std::sin(theta)
                    };

                    Vector2 screenPos;
                    if (GameViewWindow::WorldToScreen(circleWorldPos, camera, screenPos, vpOffsetXRatio_, vpWidthRatio_)) {
                        screenPoints.push_back(ImVec2(screenPos.x, screenPos.y));
                    } else {
                        allPointsVisible = false;
                    }
                }

                // 全頂点がカメラ手前に投影できている場合は閉じたポリラインと半透明塗りつぶし
                if (allPointsVisible && static_cast<int>(screenPoints.size()) == kCircleSegments) {
                    drawList->AddConvexPolyFilled(screenPoints.data(), static_cast<int>(screenPoints.size()), fillColor);
                    drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()), circleColor, ImDrawFlags_Closed, isSelected ? kSelectedCircleLineWidth : kCircleLineWidth);
                } else if (screenPoints.size() >= 2) {
                    // 一部がクリップされている場合は開いたポリラインとして安全に描画
                    drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()), circleColor, ImDrawFlags_None, isSelected ? kSelectedCircleLineWidth : kCircleLineWidth);
                }
            }
        }

        // --------------------------------------------------
        // 2. 3Dテキスト（ビルボードバッジ）描画 (show3DLabels_ が有効な場合)
        // --------------------------------------------------
        if (!show3DLabels_) continue;

        // 地面中心のスクリーン座標
        Vector2 groundScreenPos;
        bool groundVisible = GameViewWindow::WorldToScreen(pos, camera, groundScreenPos, vpOffsetXRatio_, vpWidthRatio_);

        // ラベル位置（頭上）のスクリーン座標
        Vector3 labelWorldPos = pos;
        labelWorldPos.y += kLabelHeightOffset;
        Vector2 labelScreenPos;
        bool labelVisible = GameViewWindow::WorldToScreen(labelWorldPos, camera, labelScreenPos, vpOffsetXRatio_, vpWidthRatio_);

        if (!labelVisible) continue;

        // ラベルテキストの構築
        std::string typeName = "通常突進";
        switch (enemy->GetEnemyType()) {
        case Enemy::EnemyType::Normal:      typeName = "通常突進"; break;
        case Enemy::EnemyType::Stationary:  typeName = "固定砲台"; break;
        case Enemy::EnemyType::Swarm:       typeName = "追従群生"; break;
        case Enemy::EnemyType::Boss:         typeName = "BOSS"; break;
        }

        std::string labelText;
        if (isSelected) {
            labelText += "★[選択中]★ ";
        }
        labelText += "[" + typeName + "] ×" + std::to_string(enemy->GetSpawnCount());

        char radiusBuf[32];
        snprintf(radiusBuf, sizeof(radiusBuf), " R:%.1fm", enemy->GetSpawnRadius());
        labelText += radiusBuf;

        if (enemy->GetEnemyType() == Enemy::EnemyType::Swarm) {
            char hangBuf[32];
            snprintf(hangBuf, sizeof(hangBuf), " (%.1fs)", enemy->GetHangTime());
            labelText += hangBuf;
        }

        // テキストサイズの取得
        ImVec2 textSize = ImGui::CalcTextSize(labelText.c_str());
        ImVec2 badgeMin = ImVec2(labelScreenPos.x - textSize.x * 0.5f - kBadgePaddingX, labelScreenPos.y - textSize.y * 0.5f - kBadgePaddingY);
        ImVec2 badgeMax = ImVec2(labelScreenPos.x + textSize.x * 0.5f + kBadgePaddingX, labelScreenPos.y + textSize.y * 0.5f + kBadgePaddingY);

        // 地面中心からバッジ下部への引き出し線
        if (groundVisible) {
            ImVec2 badgeBottom = ImVec2(labelScreenPos.x, badgeMax.y);
            drawList->AddLine(ImVec2(groundScreenPos.x, groundScreenPos.y), badgeBottom, isSelected ? kBadgeSelectedBorderColor : kLineColor, kLeadLineWidth);
            drawList->AddCircleFilled(ImVec2(groundScreenPos.x, groundScreenPos.y), kGroundMarkerRadius, isSelected ? kBadgeSelectedBorderColor : kLineColor);
        }

        // バッジ背景
        drawList->AddRectFilled(badgeMin, badgeMax, kBadgeBgColor, kBadgeRounding);
        // バッジ枠線
        drawList->AddRect(badgeMin, badgeMax, isSelected ? kBadgeSelectedBorderColor : kBadgeBorderColor, kBadgeRounding, 0, isSelected ? kSelectedBorderWidth : kNormalBorderWidth);

        // テキスト描画
        ImVec2 textPos = ImVec2(labelScreenPos.x - textSize.x * 0.5f, labelScreenPos.y - textSize.y * 0.5f);
        drawList->AddText(textPos, isSelected ? kSelectedTextColor : kTextColor, labelText.c_str());
    }

    // --------------------------------------------------
    // 3. 配置モード（Placing）時のリアルタイム 3D カーソルプレビュー描画
    // --------------------------------------------------
    if (editMode_ == EditMode::Placing && isCursorOnGround_) {
        // プレビュー湧きサークル
        std::vector<ImVec2> previewPoints;
        previewPoints.reserve(kCircleSegments);
        bool allPointsVisible = true;
        static constexpr float kTwoPi = 6.28318530718f;
        float angleStep = kTwoPi / static_cast<float>(kCircleSegments);

        for (int s = 0; s < kCircleSegments; ++s) {
            float theta = static_cast<float>(s) * angleStep;
            Vector3 circleWorldPos = {
                cursorGroundPos_.x + pendingRadius_ * std::cos(theta),
                cursorGroundPos_.y + kCircleElevation,
                cursorGroundPos_.z + pendingRadius_ * std::sin(theta)
            };

            Vector2 screenPos;
            if (GameViewWindow::WorldToScreen(circleWorldPos, camera, screenPos, vpOffsetXRatio_, vpWidthRatio_)) {
                previewPoints.push_back(ImVec2(screenPos.x, screenPos.y));
            } else {
                allPointsVisible = false;
            }
        }

        if (allPointsVisible && static_cast<int>(previewPoints.size()) == kCircleSegments) {
            drawList->AddConvexPolyFilled(previewPoints.data(), static_cast<int>(previewPoints.size()), kPreviewCircleFillColor);
            drawList->AddPolyline(previewPoints.data(), static_cast<int>(previewPoints.size()), kPreviewCircleColor, ImDrawFlags_Closed, kSelectedCircleLineWidth);
        } else if (previewPoints.size() >= 2) {
            drawList->AddPolyline(previewPoints.data(), static_cast<int>(previewPoints.size()), kPreviewCircleColor, ImDrawFlags_None, kSelectedCircleLineWidth);
        }

        // プレビューバッジ（頭上）
        Vector3 previewLabelPos = cursorGroundPos_;
        previewLabelPos.y += kLabelHeightOffset;
        Vector2 previewScreenPos;
        if (GameViewWindow::WorldToScreen(previewLabelPos, camera, previewScreenPos, vpOffsetXRatio_, vpWidthRatio_)) {
            bool isPaused = false;
            if (auto debugEditor = Zuizui::GetInstance()->GetDebugEditor()) {
                isPaused = debugEditor->IsPaused();
            }

            std::string previewText;
            if (isPaused) {
                char radiusBuf[32];
                snprintf(radiusBuf, sizeof(radiusBuf), "%.1f", pendingRadius_);
                previewText = "+ [左クリックで配置] 半径:" + std::string(radiusBuf) + "m (ホイールで拡縮)";
            } else {
                previewText = "※ 一時停止中のみ配置可能 (上部ボタンで一時停止してください)";
            }

            ImVec2 previewTextSize = ImGui::CalcTextSize(previewText.c_str());
            ImVec2 pBadgeMin = ImVec2(previewScreenPos.x - previewTextSize.x * 0.5f - kBadgePaddingX, previewScreenPos.y - previewTextSize.y * 0.5f - kBadgePaddingY);
            ImVec2 pBadgeMax = ImVec2(previewScreenPos.x + previewTextSize.x * 0.5f + kBadgePaddingX, previewScreenPos.y + previewTextSize.y * 0.5f + kBadgePaddingY);

            drawList->AddRectFilled(pBadgeMin, pBadgeMax, kPreviewBadgeBgColor, kBadgeRounding);
            drawList->AddRect(pBadgeMin, pBadgeMax, kPreviewBadgeBorderColor, kBadgeRounding, 0, kNormalBorderWidth);
            drawList->AddText(ImVec2(previewScreenPos.x - previewTextSize.x * 0.5f, previewScreenPos.y - previewTextSize.y * 0.5f), kPreviewTextColor, previewText.c_str());
        }
    }

    drawList->PopClipRect();
}

void StageEditor::AddEnemyFrom2D(const Vector3& pos, Enemy::EnemyType type, int count, float radius, float hangTime) {
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize();
    enemy->SetPosition(pos);
    enemy->SetSpawnPoint(true);
    enemy->SetEnemyType(type);
    enemy->SetSpawnCount(count);
    enemy->SetSpawnRadius(radius);
    enemy->SetHangTime(hangTime);

    editorEnemies_.push_back(std::move(enemy));
    SetSelectedIndex(static_cast<int>(editorEnemies_.size()) - 1);
    SceneHierarchy::GetInstance()->SetSelected(editorEnemies_.back()->GetCube());
}

void StageEditor::DeleteEnemy(int index) {
    if (index < 0 || index >= static_cast<int>(editorEnemies_.size())) return;

    if (SceneHierarchy::GetInstance()->GetSelected() == editorEnemies_[index]->GetCube()) {
        SceneHierarchy::GetInstance()->SetSelected(nullptr);
    }
    editorEnemies_.erase(editorEnemies_.begin() + index);
    if (selectedEnemyIndex_ == index) {
        SetSelectedIndex(-1);
    } else if (selectedEnemyIndex_ > index) {
        SetSelectedIndex(selectedEnemyIndex_ - 1);
    }
}
#endif

void StageEditor::ImGuiControl() {
#ifdef _USEIMGUI
    // ポーズ中であってもエディタ配置・同期処理を確実に更新
    Update();

    BaseCamera* camera = CameraResource::GetCameraManager()->GetActiveCamera();
    Update3DPlacement(camera);

    // 3D空間テキスト（ビルボードバッジ）オーバーレイ描画
    Draw3DLabels();

    if (!showStageEditor_) return;

    if (ImGui::Begin("敵エディタ###Enemy Editor", &showStageEditor_)) {

    // --- ゲーム状態ステータスバナー & 一時停止トグル ---
    bool isPaused = false;
    auto debugEditor = Zuizui::GetInstance()->GetDebugEditor();
    if (debugEditor) {
        isPaused = debugEditor->IsPaused();
    }

    if (isPaused) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "● ゲーム一時停止中（赤枠画面）: 2Dまたは3D地面をクリックして敵を設置可能！");
        if (debugEditor) {
            ImGui::SameLine();
            if (ImGui::Button("▶ ゲームを再開")) {
                debugEditor->SetPause(false);
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "● ゲーム進行中（青枠画面）: 敵を配置するにはゲームを一時停止してください");
        if (debugEditor) {
            ImGui::SameLine();
            if (ImGui::Button("|| ゲームを一時停止して配置可能にする")) {
                debugEditor->SetPause(true);
            }
        }
    }

    ImGui::Separator();

    // 0. ガイド表示トグル
    ImGui::Text("ガイド表示設定:");
    ImGui::Checkbox("湧き範囲サークル表示", &showSpawnCircles_);
    ImGui::SameLine();
    ImGui::Checkbox("3Dテキスト表示", &show3DLabels_);
    ImGui::SameLine();
    ImGui::Checkbox("プレイ中も表示", &showDuringPlay_);

    ImGui::Separator();

    // 1. モード選択
    ImGui::Text("操作モード:");
    int modeInt = static_cast<int>(editMode_);
    if (ImGui::RadioButton("通常 (None)", &modeInt, 0)) editMode_ = EditMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("配置モード (Place)", &modeInt, 1)) editMode_ = EditMode::Placing;
    ImGui::SameLine();
    if (ImGui::RadioButton("削除モード (Delete)", &modeInt, 2)) editMode_ = EditMode::Deleting;
    ImGui::SameLine();
    if (ImGui::RadioButton("編集モード (Edit)", &modeInt, 3)) editMode_ = EditMode::Editing;

    ImGui::Separator();

    // 2. 配置設定 (Placing用)
    ImGui::Text("配置パラメータ設定:");
    const char* typeNames[] = { "通常突進 (Normal)", "固定砲台 (Stationary)", "追従群生 (Swarm)" };
    int currentTypeIdx = static_cast<int>(pendingType_);
    if (ImGui::Combo("敵タイプ", &currentTypeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
        pendingType_ = static_cast<Enemy::EnemyType>(currentTypeIdx);
    }

    ImGui::SliderInt("スポーン数", &pendingCount_, 1, 10);
    ImGui::SliderFloat("スポーン半径", &pendingRadius_, kMinSpawnRadius, kMaxSpawnRadius);
    if (pendingType_ == Enemy::EnemyType::Swarm) {
        ImGui::SliderFloat("滞空時間 (秒)", &pendingHangTime_, 1.0f, 10.0f);
    }

    if (editMode_ == EditMode::Placing) {
        if (isPaused) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "状態: 2Dマップ または 3D空間の地面を左クリックして敵を配置！");
            ImGui::Text("3D地面上をホバーしながらマウスホイールで湧き半径を調整できます。");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "注意: ゲームが動いています。上の「|| 一時停止」ボタンを押して停止させてください。");
        }
    } else {
        if (ImGui::Button("配置モード開始 (2D/3Dマップをクリックで配置)")) {
            editMode_ = EditMode::Placing;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("カメラ注視点に敵を追加")) {
        BaseCamera* cam = CameraResource::GetCameraManager()->GetActiveCamera();
        Vector3 spawnPos = cam ? cam->GetTarget() : Vector3{ 0.0f, 0.0f, -220.0f };
        spawnPos.y = kGroundLevelY;
        AddEnemyFrom2D(spawnPos, pendingType_, pendingCount_, pendingRadius_, pendingHangTime_);
    }

    ImGui::Separator();
    ImGui::Text("配置済み敵リスト:");

    // 敵のリスト表示
    for (int i = 0; i < static_cast<int>(editorEnemies_.size()); ++i) {
        auto& e = editorEnemies_[i];
        std::string typeStr = "通常";
        if (e->GetEnemyType() == Enemy::EnemyType::Stationary) typeStr = "固定";
        else if (e->GetEnemyType() == Enemy::EnemyType::Swarm) typeStr = "群生";

        std::string label = "ポイント " + std::to_string(i) + " [" + typeStr + " (数:" + std::to_string(e->GetSpawnCount()) + ")]";
        bool isSelected = (selectedEnemyIndex_ == i);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            SetSelectedIndex(i);
            editMode_ = EditMode::Editing;
            SceneHierarchy::GetInstance()->SetSelected(e->GetCube());
        }
    }

    // 選択中の敵の編集
    if (selectedEnemyIndex_ >= 0 && selectedEnemyIndex_ < static_cast<int>(editorEnemies_.size())) {
        ImGui::Separator();
        ImGui::Text("選択中の敵の詳細設定:");
        auto& enemy = editorEnemies_[selectedEnemyIndex_];

        Vector3 pos = enemy->GetPosition();
        if (ImGui::DragFloat3("座標", &pos.x, 0.1f)) {
            enemy->SetPosition(pos);
        }

        int typeIdx = static_cast<int>(enemy->GetEnemyType());
        if (ImGui::Combo("敵タイプ##Selected", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
            enemy->SetEnemyType(static_cast<Enemy::EnemyType>(typeIdx));
        }

        int count = enemy->GetSpawnCount();
        if (ImGui::SliderInt("スポーン数##Selected", &count, 1, 10)) {
            enemy->SetSpawnCount(count);
        }

        float radius = enemy->GetSpawnRadius();
        if (ImGui::SliderFloat("スポーン半径##Selected", &radius, 1.0f, 20.0f)) {
            enemy->SetSpawnRadius(radius);
        }

        if (enemy->GetEnemyType() == Enemy::EnemyType::Swarm) {
            float hang = enemy->GetHangTime();
            if (ImGui::SliderFloat("滞空時間##Selected", &hang, 1.0f, 10.0f)) {
                enemy->SetHangTime(hang);
            }
        }

        if (ImGui::Button("選択中の敵を削除")) {
            DeleteEnemy(selectedEnemyIndex_);
        }
    }

    // セーブ・ロード
    ImGui::Separator();
    if (ImGui::Button("ステージ保存")) {
        SaveStage(kStageFilePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("ステージ読込")) {
        LoadStage(kStageFilePath);
    }
    }
    ImGui::End();
#endif
}

void StageEditor::SaveStage(const std::string& filepath) {
    _mkdir("resources");
    _mkdir("resources/stages");

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return;

    ofs << "[\n";
    for (size_t i = 0; i < editorEnemies_.size(); ++i) {
        auto& enemy = editorEnemies_[i];
        Vector3 pos = enemy->GetPosition();
        int typeInt = static_cast<int>(enemy->GetEnemyType());
        int count = enemy->GetSpawnCount();
        float radius = enemy->GetSpawnRadius();
        float hang = enemy->GetHangTime();

        ofs << "  {\"pos\": [" << pos.x << ", " << pos.y << ", " << pos.z << "], "
            << "\"type\": " << typeInt << ", "
            << "\"count\": " << count << ", "
            << "\"radius\": " << radius << ", "
            << "\"hang_time\": " << hang << "}";
        if (i + 1 < editorEnemies_.size()) {
            ofs << ",";
        }
        ofs << "\n";
    }
    ofs << "]\n";
}

namespace {
    constexpr float kDefaultSpawnRadius = 5.0f;
    constexpr float kDefaultHangTime = 3.0f;
    constexpr int kDefaultSpawnCount = 1;

    const std::string kKeyPos = "\"pos\": [";
    const std::string kKeyType = "\"type\": ";
    const std::string kKeyCount = "\"count\": ";
    const std::string kKeySize = "\"size\": [";
    const std::string kKeyRadius = "\"radius\": ";
    const std::string kKeyHangTime = "\"hang_time\": ";
}

void StageEditor::LoadStage(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    editorEnemies_.clear();
    SetSelectedIndex(-1);
#ifdef _USEIMGUI
    SceneHierarchy::GetInstance()->SetSelected(nullptr);
#endif

    std::string line;
    while (std::getline(ifs, line)) {
        size_t posIdx = line.find(kKeyPos);
        if (posIdx == std::string::npos) continue;

        size_t posEnd = line.find("]", posIdx);
        if (posEnd == std::string::npos) continue;

        size_t posStart = posIdx + kKeyPos.length();
        if (posStart >= posEnd) continue;

        std::string posStr = line.substr(posStart, posEnd - posStart);
        float px = 0.0f, py = 0.0f, pz = 0.0f;
        if (sscanf_s(posStr.c_str(), "%f, %f, %f", &px, &py, &pz) != 3) continue;

        int typeInt = 0;
        size_t typeIdx = line.find(kKeyType);
        if (typeIdx != std::string::npos) {
            size_t valStart = typeIdx + kKeyType.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%d", &typeInt);
            }
        }

        int count = kDefaultSpawnCount;
        size_t countIdx = line.find(kKeyCount);
        if (countIdx != std::string::npos) {
            size_t valStart = countIdx + kKeyCount.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%d", &count);
            }
        } else {
            // 旧形式サイズデータからのフォールバック
            size_t sizeIdx = line.find(kKeySize);
            if (sizeIdx != std::string::npos) {
                size_t valStart = sizeIdx + kKeySize.length();
                if (valStart < line.length()) {
                    float sx = 1.0f;
                    sscanf_s(line.c_str() + valStart, "%f", &sx);
                    count = static_cast<int>(sx);
                }
            }
        }

        float radius = kDefaultSpawnRadius;
        size_t radiusIdx = line.find(kKeyRadius);
        if (radiusIdx != std::string::npos) {
            size_t valStart = radiusIdx + kKeyRadius.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%f", &radius);
            }
        }

        float hang = kDefaultHangTime;
        size_t hangIdx = line.find(kKeyHangTime);
        if (hangIdx != std::string::npos) {
            size_t valStart = hangIdx + kKeyHangTime.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%f", &hang);
            }
        }

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize();
        enemy->SetPosition({ px, py, pz });
        enemy->SetSpawnPoint(true);
        enemy->SetEnemyType(static_cast<Enemy::EnemyType>(typeInt));
        enemy->SetSpawnCount(count);
        enemy->SetSpawnRadius(radius);
        enemy->SetHangTime(hang);

        editorEnemies_.push_back(std::move(enemy));
    }
}
