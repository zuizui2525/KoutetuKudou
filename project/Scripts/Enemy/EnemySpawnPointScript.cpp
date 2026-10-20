#include "Scripts/Enemy/EnemySpawnPointScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/PrefabManager.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
#include <numbers>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemySpawnPointScript)

EnemySpawnPointScript::EnemySpawnPointScript()
    : ScriptComponent("EnemySpawnPointScript") {
}

void EnemySpawnPointScript::Awake() {
    hasTriggered_ = false;

    // Transformの初期Z座標からトリガーZの初期値を自動計算（未設定の場合）
    if (owner_ && autoSyncTriggerZWithPosition_ && triggerZ_ == 0.0f) {
        float currentZ = owner_->GetPosition().z;
        triggerZ_ = currentZ - kDefaultTriggerDistanceAhead;
        if (triggerZ_ < 0.0f) triggerZ_ = 0.0f;
    }
}

void EnemySpawnPointScript::Start() {
}

void EnemySpawnPointScript::OnUpdate() {
    // 走行中のスポーン判定は EnemySpawnManagerScript から一括して CheckAndTrigger() が呼び出される
}

/// <summary>
/// ボス設定のフラグを切り替え、プレハブ名やデフォルト設定を同期します。
/// </summary>
/// <param name="boss">ボスの場合は true</param>
void EnemySpawnPointScript::SetBoss(bool boss) {
    isBoss_ = boss;
    prefabName_ = isBoss_ ? kBossEnemyPrefab : kDefaultEnemyPrefab;
    if (isBoss_) {
        count_ = 1;
        targetArea_ = 3; // エリア4（0起点で3）
    }
}

/// <summary>
/// 自機のZ進行度がトリガーラインを越えているか判定し、越えていれば敵を生成します。
/// </summary>
bool EnemySpawnPointScript::CheckAndTrigger(float playerPosZ, int currentArea) {
    if (hasTriggered_) return false;
    if (targetArea_ != currentArea) return false;

    if (playerPosZ >= triggerZ_) {
        SpawnEnemies();
        hasTriggered_ = true;
        return true;
    }
    return false;
}

/// <summary>
/// 設定されたパラメータ（プレハブ名、個数、分散半径）に基づいて、
/// 自身のTransform座標を中心とした円周上に敵GameObjectを動的生成してシーンへ登録します。
/// </summary>
std::vector<GameObject*> EnemySpawnPointScript::SpawnEnemies() {
    std::vector<GameObject*> spawnedList;
    if (!owner_) return spawnedList;

    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return spawnedList;

    Vector3 centerPos = owner_->GetPosition();
    Vector3 baseRot = owner_->GetRotate();

    for (int i = 0; i < count_; ++i) {
        Vector3 spawnPos = centerPos;

        // 複数出現時の円形分散オフセット計算
        if (count_ > 1 && spreadRadius_ > 0.0f) {
            float angle = (static_cast<float>(i) / static_cast<float>(count_)) * 2.0f * std::numbers::pi_v<float>;
            spawnPos.x += std::cos(angle) * spreadRadius_;
            spawnPos.z += std::sin(angle) * spreadRadius_;
        }

        // プレハブから新規GameObjectをインスタンス化
        auto enemyGo = PrefabManager::GetInstance().Instantiate(prefabName_, spawnPos, baseRot);
        if (enemyGo) {
            GameObject* rawPtr = enemyGo.get();
            scene->AddGameObject(std::move(enemyGo));
            spawnedList.push_back(rawPtr);
        }
    }

    return spawnedList;
}

void EnemySpawnPointScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.6f, 0.2f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵スポーン地点設定 (Spawn Point)】");
    ImGui::Separator();

    // 1. 基本設定
    ImGui::Text("出現状態: %s", hasTriggered_ ? "出現済み (Triggered)" : "待機中 (Ready)");
    if (hasTriggered_) {
        ImGui::SameLine();
        if (ImGui::Button("リセット##ResetTrigger")) {
            ResetTrigger();
        }
    }

    // ボス設定
    if (ImGui::Checkbox("ボス出現ポイント##IsBoss", &isBoss_)) {
        SetBoss(isBoss_);
    }

    // プレハブ選択
    const char* prefabs[] = { "Enemy", "BossEnemy" };
    int currentPrefabIdx = (prefabName_ == kBossEnemyPrefab) ? 1 : 0;
    if (ImGui::Combo("出現プレハブ##PrefabSelect", &currentPrefabIdx, prefabs, IM_ARRAYSIZE(prefabs))) {
        SetBoss(currentPrefabIdx == 1);
    }

    // 所属エリア
    ImGui::SliderInt("所属エリア##TargetArea", &targetArea_, 0, 3, "Area %d (0~3)");

    // 出現トリガーZ
    ImGui::DragFloat("出現トリガーZライン##TriggerZ", &triggerZ_, 1.0f, 0.0f, 1000.0f, "%.1f m");
    if (owner_) {
        float ownerZ = owner_->GetPosition().z;
        ImGui::TextDisabled("配置位置Z: %.1f m (トリガーまで: %.1f m)", ownerZ, ownerZ - triggerZ_);
        if (ImGui::Button("配置位置の25m手前にトリガーを合わせる##AlignTrigger")) {
            triggerZ_ = ownerZ - kDefaultTriggerDistanceAhead;
            if (triggerZ_ < 0.0f) triggerZ_ = 0.0f;
        }
    }

    // 出現数と分散
    if (!isBoss_) {
        ImGui::SliderInt("同時出現数##SpawnCount", &count_, kMinSpawnCount, kMaxSpawnCount, "%d 体");
        if (count_ > 1) {
            ImGui::DragFloat("分散半径##SpreadRadius", &spreadRadius_, 0.2f, kMinSpreadRadius, kMaxSpreadRadius, "%.1f m");
        }
    }

    ImGui::Separator();

    // 2. エディタプレビュー機能
    if (ImGui::Button("▶ この地点の敵をテスト生成##TestSpawnBtn")) {
        SpawnEnemies();
    }
#endif
}
