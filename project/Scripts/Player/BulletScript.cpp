#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
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

REGISTER_SCRIPT(BulletScript)

BulletScript::BulletScript()
    : ScriptComponent("BulletScript") {
}

void BulletScript::Awake() {
    currentLifeTimer_ = 0;
    isDead_ = false;
    hasPreviousPos_ = false;
}

void BulletScript::Start() {
    if (owner_) {
        auto* gpuParticle = owner_->GetComponent<GPUParticleComponent>();
        if (!gpuParticle) {
            gpuParticle = owner_->AddComponent<GPUParticleComponent>();
        }
        if (gpuParticle) {
            gpuParticle->ApplyPreset("BulletTrail");
            if (auto* obj = gpuParticle->GetParticleObject()) {
                auto& s = obj->GetSetting();

                // 連射時の同一形状化を打破するための微小ジッター付与 (マジックナンバー排除)
                // 弾丸ごとに速度拡散と寿命をばらつかせ、1発ごとに有機的で異なる軌跡を生成
                float randJitter = static_cast<float>(rand() % 100) / 100.0f; // 0.0f 〜 1.0f
                constexpr float kBaseVelocityRange = 0.05f;
                constexpr float kJitterVelocityScale = 0.04f;
                constexpr float kBaseLifeTimeMin = 0.07f;
                constexpr float kBaseLifeTimeMax = 0.15f;
                constexpr float kJitterLifeScale = 0.05f;

                float velRange = kBaseVelocityRange + kJitterVelocityScale * randJitter;
                s.minVelocity = { -velRange, -velRange, -velRange };
                s.maxVelocity = {  velRange,  velRange,  velRange };
                s.minLifeTime = kBaseLifeTimeMin + kJitterLifeScale * randJitter;
                s.maxLifeTime = kBaseLifeTimeMax + kJitterLifeScale * randJitter;

                // 自機弾: スタイリッシュな黒煙・ダークグレートレイル / 敵弾: 赤紫系
                if (isPlayerBullet_) {
                    constexpr Vector4 kPlayerTrailStartColor = { 0.15f, 0.15f, 0.18f, 0.70f };
                    constexpr Vector4 kPlayerTrailEndColor = { 0.04f, 0.04f, 0.05f, 0.00f };
                    s.startColor = kPlayerTrailStartColor;
                    s.endColor = kPlayerTrailEndColor;
                } else {
                    constexpr Vector4 kEnemyTrailStartColor = { 1.0f, 0.2f, 0.4f, 1.0f };
                    constexpr Vector4 kEnemyTrailEndColor = { 0.8f, 0.0f, 0.2f, 0.0f };
                    s.startColor = kEnemyTrailStartColor;
                    s.endColor = kEnemyTrailEndColor;
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

    // 1. 寿命タイマーの更新
    currentLifeTimer_++;
    if (currentLifeTimer_ >= maxLifeTime_) {
        isDead_ = true;
        owner_->SetVisible(false);
        Destroy(owner_);
        return;
    }

    // 2. 弾の移動処理
    Vector3 currentPos = owner_->GetPosition();
    if (!hasPreviousPos_) {
        previousPos_ = currentPos;
        hasPreviousPos_ = true;
    }

    currentPos.x += direction_.x * speed_;
    currentPos.y += direction_.y * speed_;
    currentPos.z += direction_.z * speed_;
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
                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // 部位別判定（頭部ならヘッドショットクリティカル）
                        bool isCritical = (name.find(kHeadKeyword) != std::string::npos);

                        // 敵AIコンポーネントを対象オブジェクトから探索
                        EnemyAIScript* enemyAI = targetObj->GetComponent<EnemyAIScript>();
                        if (enemyAI) {
                            enemyAI->TakeDamage(damage_, isCritical);
                        }

                        // 命中フラグ設定と弾丸の破棄
                        isDead_ = true;
                        owner_->SetVisible(false);

                        // 敵命中爆発エフェクトの発生
                        EffectPlayParam hitParam{};
                        hitParam.position = bulletPos;
                        hitParam.scale = { kBulletHitScale, kBulletHitScale, kBulletHitScale };
                        hitParam.isLoop = false;
                        if (auto* effectMgr = EffectManager::GetInstance()) {
                            effectMgr->PlayEffect3D(kEffectBulletHit, hitParam);
                        }

                        float shakeIntensity = isCritical ? kCriticalShakeIntensity : kHitShakeIntensity;
                        float shakeDuration = isCritical ? kCriticalShakeDuration : kHitShakeDuration;
                        ShakeCamera(shakeIntensity, shakeDuration);
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
    ImGui::DragFloat("当たり判定半径##ColRadius", &collisionRadius_, 0.02f, 0.05f, 2.0f, "%.2f");
    ImGui::DragInt("威力##Damage", &damage_, 1, 1, 100);
    ImGui::Checkbox("自機弾フラグ##IsPlayerBullet", &isPlayerBullet_);

    ImGui::Text("寿命進行: %d / %d", currentLifeTimer_, maxLifeTime_);
    ImGui::Text("状態: %s", isDead_ ? "消滅(Dead)" : "アクティブ(Alive)");
#endif
}
