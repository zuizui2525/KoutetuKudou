#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/ReticleControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Math/Collision/Collision.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Component/Components/GPUParticleComponent.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

#include <cmath>
#include <algorithm>

REGISTER_SCRIPT(BulletScript)

BulletScript::BulletScript()
    : ScriptComponent("BulletScript") {
}

void BulletScript::Awake() {
    currentLifeTimer_ = 0;
    isDead_ = false;
    hasPreviousPos_ = false;
    pierceCount_ = 0;
    hitTargets_.clear();
}

void BulletScript::Start() {
    if (owner_) {
        // 弾丸の見た目スケールを設定（自機弾: VALORANT風一本線レーザービーム）
        owner_->SetScale(visualScale_);

        // 自機弾は細かい粒々パーティクルを一切噴射せず、一本のシャープな光線ビームとして飛翔！
        if (!isPlayerBullet_) {
            auto* gpuParticle = owner_->GetComponent<GPUParticleComponent>();
            if (!gpuParticle) {
                gpuParticle = owner_->AddComponent<GPUParticleComponent>();
            }
            if (gpuParticle) {
                gpuParticle->ApplyPreset("BulletTrail");
                if (auto* obj = gpuParticle->GetParticleObject()) {
                    auto& s = obj->GetSetting();
                    constexpr Vector4 kEnemyTrailStartColor = { 1.0f, 0.2f, 0.4f, 1.0f };
                    constexpr Vector4 kEnemyTrailEndColor = { 0.8f, 0.0f, 0.2f, 0.0f };
                    s.startColor = kEnemyTrailStartColor;
                    s.endColor = kEnemyTrailEndColor;
                }
            }
        } else {
            // 自機弾の場合、既存のGPUParticleComponentがあれば自動噴射を完全停止
            if (auto* gpuParticle = owner_->GetComponent<GPUParticleComponent>()) {
                gpuParticle->SetSpawnRate(0.0f);
                if (auto* obj = gpuParticle->GetParticleObject()) {
                    auto& s = obj->GetSetting();
                    s.isAutoSpawning = false;
                    s.spawnRatePerSec = 0.0f;
                }
            }
        }
    }
}

void BulletScript::SetDirection(const Vector3& direction) {
    direction_ = direction;
    if (owner_) {
        float yaw = std::atan2(direction.x, direction.z);
        float horizontalDist = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        float pitch = std::atan2(-direction.y, horizontalDist);
        owner_->SetRotate({ pitch, yaw, 0.0f });
    }
}

namespace {
    constexpr float kHitShakeIntensity = 0.05f;       // 通常ヒット時の反動シェイク振幅
    constexpr float kHitShakeDuration = 0.10f;        // 通常ヒット持続時間
    constexpr float kCriticalShakeIntensity = 0.12f;  // ヘッドショットクリティカル時の強力な反動シェイク振幅
    constexpr float kCriticalShakeDuration = 0.18f;   // クリティカル持続時間
    constexpr float kPlayerHurtShakeIntensity = 0.16f;// 自機被弾時の衝撃シェイク振幅
    constexpr float kPlayerHurtShakeDuration = 0.22f; // 被弾衝撃持続時間

    // 当たり判定対象の識別キーワード
    const std::string kHeadKeyword = "Head";
    const std::string kBodyKeyword = "Body";
    const std::string kEnemyKeyword = "Enemy";
    const std::string kBossKeyword = "Boss";
    const std::string kPlayerKeyword = "Player";
}

void BulletScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // レベルアップ3択選択中は弾丸の寿命・移動・当たり判定を完全一時停止
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsLevelUpChoosing()) {
            return;
        }
    }

    // 1. 寿命タイマーの更新
    currentLifeTimer_++;
    if (currentLifeTimer_ >= maxLifeTime_) {
        isDead_ = true;
        owner_->SetVisible(false);
        Destroy(owner_);
        return;
    }

    // 2. 弾の移動処理（射出方向速度 + 自機移動の慣性ベクトル）
    Vector3 currentPos = owner_->GetPosition();
    if (!hasPreviousPos_) {
        previousPos_ = currentPos;
        hasPreviousPos_ = true;
    }

    currentPos.x += direction_.x * speed_ + inheritedVelocity_.x;
    currentPos.y += direction_.y * speed_ + inheritedVelocity_.y;
    currentPos.z += direction_.z * speed_ + inheritedVelocity_.z;
    owner_->SetPosition(currentPos);

    // 弾丸の軌跡（BulletTrail）は owner_ にアタッチされた GPUParticleComponent が
    // 各弾丸の座標に追従して完全独立したGPUパーティクルシミュレーションおよび描画を実行します。
    previousPos_ = currentPos;

    // 3. 衝突判定の確認
    CheckCollisions();
}

/// <summary>
/// 弾丸の球体コライダーとシーン上の各オブジェクトのAABB（軸平行境界ボックス）との幾何交差判定を実行します。
/// 
/// 【処理の流れ】
/// 1. 自身の座標と半径から Sphere 構造体を構築。
/// 2. シーン内のアクティブな GameObject を走査し、自身と同一または非表示のオブジェクトを除外。
/// 3. 自機弾（isPlayerBullet_ == true）の場合:
///    - 敵関連（Enemy, Boss, Head, Body）の BoxColliderComponent を検出。
///    - IsCollision(AABB, Sphere) で衝突を検知した場合、部位が "Head" ならばクリティカル判定とし、
///      対象オブジェクトまたはその親オブジェクトから EnemyAIScript を取得して TakeDamage(damage_, isCritical) を呼出。
/// 4. 敵弾（isPlayerBullet_ == false）の場合:
///    - 自機（Player）の BoxColliderComponent を検出。
///    - 衝突時に PlayerControllerScript::TakeDamage(damage_) を呼出。
/// 5. 命中時は反動カメラシェイクを誘発し、自身の非表示フラグを設定してシーンから破棄（Destroy）。
/// </summary>
void BulletScript::CheckCollisions() {
    if (!owner_ || isDead_) return;

    Vector3 bulletPos = owner_->GetPosition();

    // 建物（ビル）との衝突遮蔽判定（壁に着弾して即座に消滅）
    if (MapRendererScript::IsInsideBuilding(bulletPos, collisionRadius_)) {
        isDead_ = true;
        owner_->SetVisible(false);

        // 建物着弾時も敵命中時と統一したシャープな小粒火花スパーク（kHitGpuSparkCount）を発生
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(bulletPos, kHitGpuSparkCount);
        }

        ShakeCamera(kHitShakeIntensity * 0.5f, kHitShakeDuration);
        Destroy(owner_);
        return;
    }

    Sphere bulletSphere;
    bulletSphere.center = bulletPos;
    bulletSphere.radius = collisionRadius_;

    // シーン上の GameObject 群を取得して判定
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    const auto& gameObjects = scene->GetGameObjects();

    for (const auto& targetObj : gameObjects) {
        if (!targetObj || targetObj.get() == owner_ || !targetObj->IsVisible()) {
            continue;
        }

        // 自機弾の場合：敵タグまたはEnemyAIScriptを持つオブジェクトを対象とする
        if (isPlayerBullet_) {
            auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
            if (boxCol) {
                const std::string& name = targetObj->GetName();
                bool isEnemyPart = (name.find(kEnemyKeyword) != std::string::npos ||
                                    name.find(kBossKeyword) != std::string::npos ||
                                    name.find(kHeadKeyword) != std::string::npos ||
                                    name.find(kBodyKeyword) != std::string::npos);

                if (isEnemyPart) {
                    // 貫通弾が同じ敵へ連続多段ヒットするのを防止
                    if (std::find(hitTargets_.begin(), hitTargets_.end(), targetObj.get()) != hitTargets_.end()) {
                        continue;
                    }

                    // 幽霊化（AiState::Ghost）または死亡済みの個体は通常弾がすり抜け、奥の敵を狙い撃てるようにする
                    EnemyAIScript* enemyAI = targetObj->GetComponent<EnemyAIScript>();
                    if (enemyAI && (enemyAI->IsGhost() || enemyAI->IsDead())) {
                        continue;
                    }

                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // 部位別判定（頭部ならヘッドショットクリティカル）
                        bool isCritical = (name.find(kHeadKeyword) != std::string::npos);

                        if (enemyAI) {
                            enemyAI->TakeDamage(damage_, isCritical);
                        }

                        hitTargets_.push_back(targetObj.get());

                        // レティクル中央ヒットマーカー（×印）の瞬間点灯（クリティカル時は赤）
                        ReticleControllerScript::TriggerHitMarker(isCritical);

                        // 敵命中時の視界を塞がないシャープな小粒火花スパークのみ発生（Explosion爆発は撤廃して他の敵を見やすく維持）
                        if (auto* effectMgr = EffectManager::GetInstance()) {
                            uint32_t sparkCount = isCritical ? kCriticalHitGpuSparkCount : kHitGpuSparkCount;
                            effectMgr->PlayGPUEffect(bulletPos, sparkCount);
                        }

                        float shakeIntensity = isCritical ? kCriticalShakeIntensity : kHitShakeIntensity;
                        float shakeDuration = isCritical ? kCriticalShakeDuration : kHitShakeDuration;
                        ShakeCamera(shakeIntensity, shakeDuration);

                        // 貫通回数が残っている場合は弾丸を破棄せず奥の敵まで貫通継続
                        if (pierceCount_ > 0) {
                            pierceCount_--;
                            continue;
                        }

                        // 命中フラグ設定と弾丸の破棄
                        isDead_ = true;
                        owner_->SetVisible(false);
                        Destroy(owner_);
                        break;
                    }
                }
            }
        }
        // 敵弾の場合：プレイヤーを対象とする
        else {
            const std::string& name = targetObj->GetName();
            if (name == kPlayerKeyword || name.find(kPlayerKeyword) != std::string::npos) {
                auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
                if (boxCol) {
                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // プレイヤーコントローラーを取得してダメージ適用
                        auto* playerScript = targetObj->GetComponent<PlayerControllerScript>();
                        if (playerScript) {
                            playerScript->TakeDamage(damage_);
                        }

                        // 命中フラグ設定と弾丸の破棄
                        isDead_ = true;
                        owner_->SetVisible(false);
                        ShakeCamera(kPlayerHurtShakeIntensity, kPlayerHurtShakeDuration);
                        Destroy(owner_);
                        break;
                    }
                }
            }
        }
    }
}

void BulletScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.85f, 0.3f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【弾丸スクリプト設定】");
    ImGui::Separator();

    ImGui::DragFloat("弾速##BulletSpeed", &speed_, 0.05f, kMinSpeed, kMaxSpeed, "%.2f");
    ImGui::DragInt("最大寿命##MaxLife", &maxLifeTime_, 1, kMinLifeTime, kMaxLifeTimeLimit);
    if (ImGui::DragFloat3("見た目スケール##VisualScale", &visualScale_.x, 0.02f, 0.1f, 3.0f, "%.2f")) {
        if (owner_) {
            owner_->SetScale(visualScale_);
        }
    }
    ImGui::DragFloat("当たり判定半径##ColRadius", &collisionRadius_, 0.02f, 0.05f, 2.0f, "%.2f");
    ImGui::DragInt("威力##Damage", &damage_, 1, 1, 100);
    ImGui::Checkbox("自機弾フラグ##IsPlayerBullet", &isPlayerBullet_);

    ImGui::Text("寿命進行: %d / %d", currentLifeTimer_, maxLifeTime_);
    ImGui::Text("状態: %s", isDead_ ? "消滅(Dead)" : "アクティブ(Alive)");
#endif
}
