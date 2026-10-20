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
    CheckSpawnPoints(playerPos.z);

    // 2. フォールバック定期スポーン（スポーンポイントが未配置の場合のオプション）
    if (enablePeriodicFallback_) {
        CheckPeriodicFallback(playerPos);
    }
}

/// <summary>
/// シーン上の全スポーンポイントに対して自機通過判定を実行します。
/// </summary>
void EnemySpawnManagerScript::CheckSpawnPoints(float playerPosZ) {
    if (cachedSpawnPoints_.empty()) {
        RefreshSpawnPoints();
    }

    for (auto* sp : cachedSpawnPoints_) {
        if (sp && !sp->HasTriggered()) {
            sp->CheckAndTrigger(playerPosZ, currentArea_);
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

    // 1. スポーン地点の新規作成（ワンクリック追加）
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "◆ スポーン地点の追加 (シーン上にGameObject生成)");

    float defaultZ = (selectedEditorAreaTab_ < 4) ? (selectedEditorAreaTab_ * 120.0f + 40.0f) : 40.0f;

    if (ImGui::Button("＋ 通常敵スポーン地点を追加##AddEnemySpawnBtn")) {
        Vector3 spawnPos = { 0.0f, 0.0f, defaultZ };
        CreateSpawnPoint(spawnPos, selectedEditorAreaTab_, false);
    }
    ImGui::SameLine();
    if (ImGui::Button("＋ ボス出現地点を追加##AddBossSpawnBtn")) {
        Vector3 spawnPos = { 0.0f, kBossHeightOffset, 360.0f };
        CreateSpawnPoint(spawnPos, 3, true);
    }

    // 2. プレビュー＆クリアボタン
    ImGui::SameLine();
    if (ImGui::Button("▶ 全敵プレビュー##PreviewBtn")) {
        PreviewAllSpawns();
    }
    ImGui::SameLine();
    if (ImGui::Button("■ プレビュー消去##ClearPreviewBtn")) {
        ClearPreviewEnemies();
    }

    // 3. エリア別タブ表示
    ImGui::Spacing();
    const char* areaTabs[] = { "Area 1 (0~120m)", "Area 2 (120~240m)", "Area 3 (240~360m)", "Area 4 [Boss] (360m~)" };
    if (ImGui::BeginTabBar("EnemyEditorAreaTabs")) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::BeginTabItem(areaTabs[i])) {
                selectedEditorAreaTab_ = i;

                // 該当エリアのスポーンポイント一覧テーブル
                RefreshSpawnPoints();

                ImGui::BeginChild("SpawnPointsList", ImVec2(0, 200), true);
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

                    ImGui::SameLine();
                    ImGui::Text("| トリガー: Z=%.1f", sp->GetTriggerZ());

                    ImGui::SameLine();
                    ImGui::Text("| %s x%d", sp->GetPrefabName().c_str(), sp->GetCount());

                    // 選択ボタン（押すとSceneViewでトランスフォームギズモが表示される）
                    ImGui::SameLine();
                    if (ImGui::SmallButton("選択##SelectSp")) {
                        SceneHierarchy::GetInstance()->SetSelected(go);
                    }

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
                    ImGui::TextDisabled("このエリアにはまだスポーン地点が配置されていません。上のボタンから追加してください。");
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // 4. 全トリガーリセットボタン
    if (ImGui::Button("↺ 出現トリガーを全リセット (走行テスト用)##ResetAllTriggers")) {
        ResetSpawns();
    }
#endif
}

