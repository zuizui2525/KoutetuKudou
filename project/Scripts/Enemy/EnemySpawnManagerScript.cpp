#include "Scripts/Enemy/EnemySpawnManagerScript.h"
#include "Scripts/Enemy/EnemySpawnPointScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "App/Scene/Core/SceneDirector.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Base/BaseResource.h"
#include <cstdlib>
#include <algorithm>
#include <format>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemySpawnManagerScript)

namespace {
    constexpr float kBossHeightOffset = 2.0f; // ボス生成時の地上浮遊高さ (m)
    constexpr float kDefaultSpawnOffsetZ = 20.0f; // カメラ注視点からのデフォルト前方オフセット
    constexpr float kDefaultTriggerZDistance = 25.0f; // スポーン位置からトリガーZまでのデフォルト手前距離
    const std::string kEnemyPrefabName = "Enemy";
    const std::string kBossPrefabName = "BossEnemy";
    const std::string kSpawnPointPrefix = "SpawnPoint_";
}

EnemySpawnManagerScript::EnemySpawnManagerScript()
    : ScriptComponent("EnemySpawnManagerScript") {
}

void EnemySpawnManagerScript::Awake() {
    ResetSpawns();
}

void EnemySpawnManagerScript::Start() {
    RefreshSpawnPoints();
    if (cachedSpawnPoints_.empty()) {
        SetupStage1Preset();
    }
}

void EnemySpawnManagerScript::ResetSpawns(float startZ) {
    lastSpawnZ_ = startZ;
    hasBossSpawned_ = false;

    // シーン上の全スポーンポイントの出現トリガー状態をリセット
    RefreshSpawnPoints();
    for (auto* sp : cachedSpawnPoints_) {
        if (sp) {
            sp->ResetTrigger();
        }
    }
}

void EnemySpawnManagerScript::RefreshSpawnPoints() {
    cachedSpawnPoints_.clear();

    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    for (const auto& obj : scene->GetGameObjects()) {
        if (!obj) continue;
        if (auto* sp = obj->GetComponent<EnemySpawnPointScript>()) {
            cachedSpawnPoints_.push_back(sp);
        }
    }
}

void EnemySpawnManagerScript::OnUpdate() {
    if (!enableSpawning_) return;

    // ゲーム進行フェーズが走行戦闘フェーズ(Play)でない場合はスポーン処理を行わない
    GamePhaseControllerScript* phaseCtrl = nullptr;
    if (owner_) {
        phaseCtrl = owner_->GetComponent<GamePhaseControllerScript>();
    }
    if (!phaseCtrl) {
        auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene && scene->GetDirector()) {
            phaseCtrl = scene->GetDirector()->GetScript<GamePhaseControllerScript>();
        }
    }
    if (phaseCtrl && phaseCtrl->GetCurrentPhase() != GamePhaseControllerScript::Phase::Play) {
        return;
    }

    // プレイヤーの取得
    if (!targetObject_) {
        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    targetObject_ = obj.get();
                    targetUUID_ = obj->GetUUID();
                    break;
                }
            }
        }
    }

    if (!targetObject_) return;

    Vector3 playerPos = targetObject_->GetPosition();

    // 1. シーン上に配置された個別スポーン地点（EnemySpawnPointScript）の判定
    CheckSpawnPoints(playerPos);

    // 2. フォールバック定期スポーン（スポーンポイントが未配置の場合のオプション）
    if (enablePeriodicFallback_) {
        CheckPeriodicFallback(playerPos);
    }
}

/// <summary>
/// シーン上の全スポーンポイントに対して自機の侵入・通過判定を実行します。
/// </summary>
void EnemySpawnManagerScript::CheckSpawnPoints(const Vector3& playerPos) {
    if (cachedSpawnPoints_.empty()) {
        RefreshSpawnPoints();
    }

    for (auto* sp : cachedSpawnPoints_) {
        if (sp && !sp->HasTriggered()) {
            sp->CheckAndTrigger(playerPos, currentArea_);
        }
    }
}

/// <summary>
/// スポーン地点が未配置のエリア用のフォールバック定期湧き判定
/// </summary>
void EnemySpawnManagerScript::CheckPeriodicFallback(const Vector3& playerPos) {
    if (playerPos.z - lastSpawnZ_ >= spawnIntervalZ_) {
        lastSpawnZ_ = playerPos.z;

        float sideSign = (rand() % 2 == 0) ? 1.0f : -1.0f;
        Vector3 spawnPos = {
            playerPos.x + sideSign * kSpawnOffsetSide,
            playerPos.y,
            playerPos.z + kSpawnOffsetBack
        };

        ScriptComponent::Instantiate(kEnemyPrefabName, spawnPos);
    }
}

/// <summary>
/// 新しい敵スポーン地点 GameObject を生成し、シーンへ登録して選択状態にします。
/// </summary>
GameObject* EnemySpawnManagerScript::CreateSpawnPoint(const Vector3& spawnPos, int areaIndex, bool isBoss) {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return nullptr;

    auto newObj = std::make_unique<GameObject>();
    char nameBuf[64];
    snprintf(nameBuf, sizeof(nameBuf), "%sArea%d_%s_%02zu",
        kSpawnPointPrefix.c_str(),
        areaIndex + 1,
        isBoss ? "Boss" : "Enemy",
        cachedSpawnPoints_.size() + 1);
    newObj->SetName(nameBuf);
    newObj->SetPosition(spawnPos);

    auto* spScript = newObj->AddComponent<EnemySpawnPointScript>();
    if (spScript) {
        spScript->SetTargetArea(areaIndex);
        spScript->SetBoss(isBoss);
        spScript->SetTriggerZ((std::max)(0.0f, spawnPos.z - kDefaultTriggerZDistance));
    }

    GameObject* rawPtr = newObj.get();
    scene->AddGameObject(std::move(newObj));

    RefreshSpawnPoints();

#ifdef _USEIMGUI
    // 生成直後に選択状態にしてトランスフォームギズモを表示
    SceneHierarchy::GetInstance()->SetSelected(rawPtr);
#endif

    return rawPtr;
}

/// <summary>
/// シーン上の配置済みスポーン地点に基づいて、テスト用の敵を一括生成して目視プレビューします。
/// </summary>
void EnemySpawnManagerScript::PreviewAllSpawns() {
    RefreshSpawnPoints();
    for (auto* sp : cachedSpawnPoints_) {
        if (sp) {
            sp->SpawnEnemies();
        }
    }
}

/// <summary>
/// テスト生成された敵プレビューオブジェクトを一括消去します。
/// </summary>
void EnemySpawnManagerScript::ClearPreviewEnemies() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    std::vector<GameObject*> toDestroy;
    for (const auto& obj : scene->GetGameObjects()) {
        if (!obj) continue;
        if (obj->GetComponent<EnemyAIScript>()) {
            obj->SetVisible(false);
            toDestroy.push_back(obj.get());
        }
    }

    for (auto* obj : toDestroy) {
        scene->DestroyGameObject(obj);
    }
}

/// <summary>
/// ステージ1（Area 1: 0m〜120m）用の仮敵配置プリセット（4箇所のスポーン地点）を一括構築します。
/// </summary>
void EnemySpawnManagerScript::SetupStage1Preset() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    // 既にArea 1にスポーン地点が存在するかチェック
    RefreshSpawnPoints();
    for (auto* sp : cachedSpawnPoints_) {
        if (sp && sp->GetTargetArea() == 0) {
            return; // 既に配置済みなら重複生成しない
        }
    }

    // Wave 1: 序盤の偵察敵 (Z: 25.0m, X: 3.5m, 1体)
    {
        Vector3 pos = { 3.5f, 0.0f, 25.0f };
        GameObject* go = CreateSpawnPoint(pos, 0, false);
        if (go) {
            if (auto* sp = go->GetComponent<EnemySpawnPointScript>()) {
                sp->SetTriggerZ(0.0f); // スタート直後に出現
                sp->SetCount(1);
            }
        }
    }

    // Wave 2: 左右挟撃の2体 (Z: 50.0m, 2体, 拡散半径 4.0m)
    {
        Vector3 pos = { 0.0f, 0.0f, 50.0f };
        GameObject* go = CreateSpawnPoint(pos, 0, false);
        if (go) {
            if (auto* sp = go->GetComponent<EnemySpawnPointScript>()) {
                sp->SetTriggerZ(25.0f);
                sp->SetCount(2);
                sp->SetSpreadRadius(4.0f);
            }
        }
    }

    // Wave 3: 中央小隊の3体 (Z: 80.0m, 3体, 拡散半径 5.0m)
    {
        Vector3 pos = { 0.0f, 0.0f, 80.0f };
        GameObject* go = CreateSpawnPoint(pos, 0, false);
        if (go) {
            if (auto* sp = go->GetComponent<EnemySpawnPointScript>()) {
                sp->SetTriggerZ(55.0f);
                sp->SetCount(3);
                sp->SetSpreadRadius(5.0f);
            }
        }
    }

    // Wave 4: エリア1終端ゲート前の門番 (Z: 105.0m, 2体)
    {
        Vector3 pos = { -2.0f, 0.0f, 105.0f };
        GameObject* go = CreateSpawnPoint(pos, 0, false);
        if (go) {
            if (auto* sp = go->GetComponent<EnemySpawnPointScript>()) {
                sp->SetTriggerZ(80.0f);
                sp->SetCount(2);
                sp->SetSpreadRadius(3.5f);
            }
        }
    }

    RefreshSpawnPoints();
}

void EnemySpawnManagerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.8f, 0.4f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵エディター＆スポーン管理 (Enemy Editor)】");
    ImGui::Separator();

    ImGui::Checkbox("スポーン処理有効##EnableSpawning", &enableSpawning_);
    ImGui::SameLine();
    ImGui::Checkbox("自動定期湧き(フォールバック)##EnableFallback", &enablePeriodicFallback_);

    ImGui::Text("現在エリア: Area %d (0~3) | 配置済みスポーン地点: %zu 箇所",
        currentArea_ + 1, cachedSpawnPoints_.size());

    ImGui::Spacing();

    // 独立ウィンドウの開閉・フォーカスボタン
    if (!isEditorWindowOpen_) {
        constexpr ImVec4 kReopenBtnColor = { 0.2f, 0.7f, 0.3f, 1.0f }; // 緑色で強調
        ImGui::PushStyleColor(ImGuiCol_Button, kReopenBtnColor);
        if (ImGui::Button("★ 敵配置パレットを再表示する (Show Palette)##ReopenPaletteBtn", ImVec2(-1.0f, kReopenButtonHeight))) {
            isEditorWindowOpen_ = true;
        }
        ImGui::PopStyleColor();
        ImGui::TextDisabled("※右上の[×]で閉じた後も、このボタンでいつでも復活できます。");
    } else {
        if (ImGui::Button("パレットを前面にフォーカス (Focus Palette)##FocusPaletteBtn")) {
            ImGui::SetWindowFocus("【敵エディター】配置パレット###EnemySpawnerPalette");
        }
        ImGui::SameLine();
        if (ImGui::Button("パレットを閉じる##ClosePaletteBtn")) {
            isEditorWindowOpen_ = false;
        }
        ImGui::TextDisabled("※敵の追加・配置は左側の「【敵エディター】配置パレット」で行えます。");
    }
#endif
}

void EnemySpawnManagerScript::OnGUI() {
#ifdef _USEIMGUI
    if (!isEditorWindowOpen_) return;

    // 誰が選択されていても、エディタ実行中に常駐する独立ウィンドウ！
    ImGui::SetNextWindowSize(ImVec2(kDefaultPaletteWidth, kDefaultPaletteHeight), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("【敵エディター】配置パレット###EnemySpawnerPalette", &isEditorWindowOpen_)) {
        DrawPaletteUI();
    }
    ImGui::End();
#endif
}

void EnemySpawnManagerScript::DrawPaletteUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.8f, 0.4f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "◆ 敵配置パレット (SceneViewと連携)");
    ImGui::TextDisabled("※リストの[Focus]を押すとSceneViewカメラが敵へ移動します");
    ImGui::Separator();

    // 1. スポーン地点の新規作成（ワンクリック追加）
    float defaultZ = (selectedEditorAreaTab_ < 4) ? (selectedEditorAreaTab_ * 120.0f + 40.0f) : 40.0f;

    if (ImGui::Button("＋ 通常敵を追加##AddEnemySpawnBtn")) {
        Vector3 spawnPos = { 0.0f, 0.0f, defaultZ };
        GameObject* go = CreateSpawnPoint(spawnPos, selectedEditorAreaTab_, false);
        if (go) {
            FocusCameraOnObject(go);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("＋ ボスを追加##AddBossSpawnBtn")) {
        Vector3 spawnPos = { 0.0f, kBossHeightOffset, 360.0f };
        GameObject* go = CreateSpawnPoint(spawnPos, 3, true);
        if (go) {
            FocusCameraOnObject(go);
        }
    }

    // 2. プレビュー＆クリアボタン
    ImGui::SameLine();
    if (ImGui::Button("▶ プレビュー##PreviewBtn")) {
        PreviewAllSpawns();
    }
    ImGui::SameLine();
    if (ImGui::Button("■ 消去##ClearPreviewBtn")) {
        ClearPreviewEnemies();
    }

    // プリセット自動配置
    constexpr ImVec4 kPresetBtnColor = { 0.2f, 0.6f, 0.8f, 1.0f };
    ImGui::PushStyleColor(ImGuiCol_Button, kPresetBtnColor);
    if (ImGui::Button("★ ステージ1仮配置を生成 (Setup Stage 1)##SetupStage1Btn")) {
        SetupStage1Preset();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Checkbox("ゾーン・トリガー線を表示##ShowZonesToggle", &showAllHazardZones_)) {
        SetAllHazardZonesVisible(showAllHazardZones_);
    }

    // 3. エリア別タブ表示
    ImGui::Spacing();
    const char* areaTabs[] = { "Area 1", "Area 2", "Area 3", "Area 4 [Boss]" };
    if (ImGui::BeginTabBar("EnemyEditorAreaTabs")) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::BeginTabItem(areaTabs[i])) {
                selectedEditorAreaTab_ = i;

                // 該当エリアのスポーンポイント一覧テーブル
                RefreshSpawnPoints();

                ImGui::BeginChild("SpawnPointsList", ImVec2(0, kSpawnListChildHeight), true);
                int indexInArea = 0;
                for (auto* sp : cachedSpawnPoints_) {
                    if (!sp || sp->GetTargetArea() != i) continue;
                    GameObject* go = sp->GetOwner();
                    if (!go) continue;

                    indexInArea++;
                    ImGui::PushID(go);

                    Vector3 pos = go->GetPosition();
                    ImGui::Text("[%02d] %s", indexInArea, go->GetName().c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("(X:%.1f, Y:%.1f, Z:%.1f)", pos.x, pos.y, pos.z);

                    // フォーカス＆選択ボタン（SceneViewカメラがその敵へジャンプし、ギズモを表示）
                    if (ImGui::SmallButton("Focus##FocusSp")) {
                        FocusCameraOnObject(go);
                    }

                    ImGui::SameLine();
                    ImGui::Text("| トリガーZ: %.1f", sp->GetTriggerZ());

                    ImGui::SameLine();
                    ImGui::Text("| %s", sp->GetSpawnCountLevelText().c_str());

                    // 削除ボタン
                    ImGui::SameLine();
                    if (ImGui::SmallButton("削除##DeleteSp")) {
                        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
                        if (scene) {
                            scene->DestroyGameObject(go);
                        }
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }

                if (indexInArea == 0) {
                    ImGui::TextDisabled("このエリアにはスポーン地点がありません。\n上の「＋ 通常敵を追加」から配置してください。");
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
#endif
}

void EnemySpawnManagerScript::FocusCameraOnObject(GameObject* targetObj) {
    if (!targetObj) return;

    Vector3 targetPos = targetObj->GetPosition();

    // 1. エディタ用カメラ（SceneViewカメラ）の注視点と位置を調整
    auto* camMgr = CameraResource::GetCameraManager();
    if (camMgr) {
        BaseCamera* editorCam = camMgr->GetCamera(CameraManager::kEditorCameraName);
        if (!editorCam) {
            editorCam = camMgr->GetActiveCamera();
        }
        if (editorCam) {
            editorCam->SetTarget(targetPos);
            Vector3 newCamPos = {
                targetPos.x,
                targetPos.y + kCameraFocusElevation,
                targetPos.z - kCameraFocusDistance
            };
            editorCam->SetPosition(newCamPos);
        }
    }

#ifdef _USEIMGUI
    // 2. オブジェクトを選択状態にしてトランスフォームギズモを表示
    SceneHierarchy::GetInstance()->SetSelected(targetObj);
#endif
}

void EnemySpawnManagerScript::SetAllHazardZonesVisible(bool visible) {
    showAllHazardZones_ = visible;
    RefreshSpawnPoints();
    for (auto* sp : cachedSpawnPoints_) {
        if (sp) {
            sp->SetShowHazardZone(visible);
        }
    }
}


