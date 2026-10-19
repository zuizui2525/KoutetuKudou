#include "Scripts/Enemy/EnemySpawnManagerScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/Scene.h"
#include "App/Scene/Core/SceneDirector.h"
#include <cstdlib>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemySpawnManagerScript)

EnemySpawnManagerScript::EnemySpawnManagerScript()
    : ScriptComponent("EnemySpawnManagerScript") {
}

void EnemySpawnManagerScript::Awake() {
    ResetSpawns();
}

void EnemySpawnManagerScript::Start() {
}

void EnemySpawnManagerScript::ResetSpawns(float startZ) {
    lastSpawnZ_ = startZ;
    hasBossSpawned_ = false;
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

    // 定期スポーンの判定
    CheckPeriodicSpawn(playerPos);

    // ボススポーンの判定
    CheckBossSpawn(playerPos);
}

void EnemySpawnManagerScript::CheckPeriodicSpawn(const Vector3& playerPos) {
    if (playerPos.z - lastSpawnZ_ >= spawnIntervalZ_) {
        lastSpawnZ_ = playerPos.z;

        float sideSign = (rand() % 2 == 0) ? 1.0f : -1.0f;
        Vector3 spawnPos = {
            playerPos.x + sideSign * kSpawnOffsetSide,
            playerPos.y,
            playerPos.z + kSpawnOffsetBack
        };

        ScriptComponent::Instantiate("Enemy", spawnPos);
    }
}

void EnemySpawnManagerScript::CheckBossSpawn(const Vector3& playerPos) {
    if (currentArea_ == kBossAreaIndex && !hasBossSpawned_ && playerPos.z >= bossSpawnZ_) {
        hasBossSpawned_ = true;

        Vector3 bossSpawnPos = {
            0.0f,
            playerPos.y + 2.0f,
            playerPos.z + kBossSpawnDistanceAhead
        };

        GameObject* bossObj = ScriptComponent::Instantiate("BossEnemy", bossSpawnPos);
        if (bossObj) {
            if (auto* ai = bossObj->GetComponent<EnemyAIScript>()) {
                ai->SetBoss(true);
            }
        }
    }
}

void EnemySpawnManagerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.8f, 0.5f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵スポーンマネージャー設定】");
    ImGui::Separator();

    ImGui::Checkbox("スポーン有効##EnableSpawning", &enableSpawning_);
    ImGui::DragFloat("定期スポーン間隔(Z)##IntervalZ", &spawnIntervalZ_, 1.0f, 5.0f, 200.0f, "%.1f m");
    ImGui::DragFloat("ボス出現ライン(Z)##BossZ", &bossSpawnZ_, 5.0f, 50.0f, 1000.0f, "%.1f m");
    ImGui::Text("現在エリア: %d (0~3)", currentArea_);
    ImGui::Text("ボス出現済み: %s", hasBossSpawned_ ? "Yes" : "No");
#endif
}
